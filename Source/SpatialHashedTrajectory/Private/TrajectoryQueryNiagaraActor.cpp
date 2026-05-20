// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "VRLogManager.h"

// ─── File-local helpers ───────────────────────────────────────────────────────

/**
 * Decode a packed periodic volume index into a world-space offset vector.
 *
 * The index uses two's-complement byte packing per axis:
 *   Bits  7.. 0 = ix,  Bits 15.. 8 = iy,  Bits 23..16 = iz
 * Each signed component is multiplied by the corresponding box-extent to
 * produce the translation that must be added to a raw sample position to
 * place it in the correct periodic image.
 *
 * Returns FVector::ZeroVector when VolumeIndex == 0 (original box) or Extent
 * is zero (non-periodic dataset), so it is safe to call unconditionally.
 */
static FVector DecodeVolumeIndexToOffset(int32 VolumeIndex, const FVector& Extent)
{
	if (VolumeIndex == 0 || Extent.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}
	int32 ix = VolumeIndex & 0xFF;
	int32 iy = (VolumeIndex >> 8)  & 0xFF;
	int32 iz = (VolumeIndex >> 16) & 0xFF;
	// Sign-extend from unsigned byte (0-255) to signed integer (-127..127).
	if (ix >= 128) ix -= 256;
	if (iy >= 128) iy -= 256;
	if (iz >= 128) iz -= 256;
	return FVector(ix * Extent.X, iy * Extent.Y, iz * Extent.Z);
}


ATrajectoryQueryNiagaraActor::ATrajectoryQueryNiagaraActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Manager = nullptr;
	NiagaraComponent = nullptr;
	NiagaraSystem = nullptr;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;
	VisualizationTimeStart = QueryTimeStart;
	VisualizationTimeEnd   = QueryTimeEnd;
	ColorEncoding = ETrajectoryColorEncoding::Time;
}

void ATrajectoryQueryNiagaraActor::BeginPlay()
{
	Super::BeginPlay();

	// Spawn the Niagara component from the assigned system asset if provided.
	if (NiagaraSystem)
	{
		NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAttached(
			NiagaraSystem,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset,
			false  // do not auto-activate – we push data first
		);
		NiagaraComponent->Deactivate();
	}
}

void ATrajectoryQueryNiagaraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cancel any in-progress query so the background worker stops as soon as
	// possible and does not attempt to access destroyed game-thread objects.
	if (Manager)
	{
		Manager->CancelActiveQuery();
	}

	Super::EndPlay(EndPlayReason);
}

void ATrajectoryQueryNiagaraActor::CancelQuery()
{
	if (Manager)
	{
		Manager->CancelActiveQuery();
	}
}

bool ATrajectoryQueryNiagaraActor::InitializeManager()
{
	if (DatasetDirectory.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("ATrajectoryQueryNiagaraActor: DatasetDirectory is not set."));
		return false;
	}

	if (!Manager)
	{
		Manager = NewObject<USpatialHashTableManager>(this);
	}

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("Initialized spatial query with cell size %f for time step %i to %i."), CellSize, QueryTimeStart, QueryTimeEnd);

	const int32 LoadedCount = Manager->LoadHashTables(
		DatasetDirectory,
		CellSize,
		QueryTimeStart,
		QueryTimeEnd,
		true  // auto-create if missing
	);

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("Loaded %i hash tables."), LoadedCount);

	if (LoadedCount == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("ATrajectoryQueryNiagaraActor: Failed to load hash tables from '%s'."), *DatasetDirectory);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("ATrajectoryQueryNiagaraActor: Loaded %d hash tables."), LoadedCount);
	return true;
}

// ─── Public BlueprintCallable entry points ────────────────────────────────────

void ATrajectoryQueryNiagaraActor::TransferDataToNiagara()
{
	if (CachedQueryPoints.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: TransferDataToNiagara called before RunQuery has completed – no data to transfer."));
		return;
	}
	TransferResultsToNiagara(CachedQueryPoints, CachedResults);
}

void ATrajectoryQueryNiagaraActor::RunQueryAndUpdateNiagara()
{
	FireAsyncQueriesWithCallback(
		FSimpleDelegate::CreateUObject(this, &ATrajectoryQueryNiagaraActor::TransferDataToNiagara));
}

void ATrajectoryQueryNiagaraActor::SetVisualizationTimeRange(int32 NewTimeStart, int32 NewTimeEnd)
{
	VisualizationTimeStart = NewTimeStart;
	VisualizationTimeEnd   = NewTimeEnd;

	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: SetVisualizationTimeRange called before a Niagara component is available."));
		return;
	}

	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"),   VisualizationTimeEnd);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Visualization time range updated to [%d, %d]."),
		VisualizationTimeStart, VisualizationTimeEnd);
}


void ATrajectoryQueryNiagaraActor::UpdateUserParameter(int32 NewTimeStart, int32 NewTimeEnd, ETrajectoryColorEncoding NewColorEncoding, ETrajectoryDistanceFilter newDistanceFilter, float newCollisionRadius, float newInnerRadius, float newOuterRadius, bool newSensitivity, bool newVisibility)
{
	VisualizationTimeStart = NewTimeStart;
	VisualizationTimeEnd = NewTimeEnd;
	ColorEncoding = NewColorEncoding;
	DistanceFilter = newDistanceFilter;
	CollisionRadius = newCollisionRadius;
	InnerRadius = newInnerRadius;
	OuterRadius = newOuterRadius;
	TimeRangeSensitivity = newSensitivity;
	ParticleVisibility = newVisibility;

	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: SetVisualizationTimeRange called before a Niagara component is available."));
		return;
	}

	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"), VisualizationTimeEnd);
	NiagaraComponent->SetVariableInt(FName("ColorEncoding"), static_cast<int32>(ColorEncoding));
	NiagaraComponent->SetVariableInt(FName("DistanceFilter"), static_cast<int32>(DistanceFilter));
	NiagaraComponent->SetVariableFloat(FName("CollisionRadius"), CollisionRadius);
	NiagaraComponent->SetVariableFloat(FName("InnerRadius"), InnerRadius);
	NiagaraComponent->SetVariableFloat(FName("OuterRadius"), OuterRadius);
	NiagaraComponent->SetVariableBool(FName("TimeRangeSensitive"), TimeRangeSensitivity);
	NiagaraComponent->SetVariableBool(FName("ParticleVisibility"), ParticleVisibility);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Updated User parameter."));
}

// ─── Core async pipeline ──────────────────────────────────────────────────────

bool ATrajectoryQueryNiagaraActor::FireAsyncQueriesWithCallback(
	FSimpleDelegate OnComplete,
	FSimpleDelegate OnFailure)
{
	if (!InitializeManager())
	{
		OnFailure.ExecuteIfBound();
		return false;
	}

	// Apply the periodic volume setting from this actor to the manager so
	// that all subsequent queries use the correct boundary conditions.
	Manager->SetPeriodicVolume(PeriodicVolume.bIsPeriodic, PeriodicVolume.Extent);

	// Cache the resolved extent so TransferResultsToNiagara can pass it to
	// Niagara as PeriodicVolumeExtent (required by the HLSL volume-index helper).
	CachedPeriodicExtent = Manager->GetResolvedPeriodicExtent(CellSize);

	if (QueryPositions.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ATrajectoryQueryNiagaraActor: QueryPositions array is empty – nothing to query."));
		OnFailure.ExecuteIfBound();
		return false;
	}

	// ── Reset cached state so progressive updates start clean ─────────────────
	// CachedQueryPointsRaw stores the original (wrapped) query positions so that
	// Niagara's QueryPoints array stays consistent with ResultPoints (both raw).
	// CachedQueryPoints stores the unwrapped (continuous) positions and is used
	// for transform computation (QueryTranslations / QueryRotations) and for the
	// query side of the bounding box.
	CachedQueryPointsRaw = QueryPositions;
	CachedQueryPoints = PeriodicVolume.bIsPeriodic
		? Manager->GetUnwrappedPositions(QueryPositions, CellSize)
		: QueryPositions;
	CachedResults.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPositionIndices.Empty();
	bBoundsValid    = false;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;

	// Note: VisualizationTimeStart, VisualizationTimeEnd and ColorEncoding are
	// intentionally NOT reset here.  They represent independent user-facing
	// controls that persist across queries, allowing the shader to keep the
	// currently selected view while new data arrives.

	const int32 NumPositions  = QueryPositions.Num();
	const int32 TimeRangeSize = QueryTimeEnd - QueryTimeStart + 1;

	if (NumPositions > TimeRangeSize)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: QueryPositions.Num() (%d) exceeds the time range [%d,%d] (%d steps). "
			     "Positions beyond index %d map to timesteps outside the loaded range and will return empty results."),
			NumPositions, QueryTimeStart, QueryTimeEnd, TimeRangeSize, TimeRangeSize - 1);
	}

	// Build the per-sample timestep array: position[i] corresponds to timestep
	// QueryTimeStart + i, clamped to QueryTimeEnd.
	TArray<int32> QueryTimeSteps;
	QueryTimeSteps.Reserve(NumPositions);
	for (int32 i = 0; i < NumPositions; ++i)
	{
		QueryTimeSteps.Add(FMath::Min(QueryTimeStart + i, QueryTimeEnd));
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Starting batched query – %d positions, outer radius %.2f, "
		     "t=[%d,%d], batch size=%d."),
		NumPositions, OuterQueryRadius, QueryTimeStart, QueryTimeEnd, BatchSize);

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
		TEXT("Starting batched query – %d positions, radius %.2f, t=[%d,%d], batch=%d."),
		NumPositions, OuterQueryRadius, QueryTimeStart, QueryTimeEnd, BatchSize);

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
		TEXT("Checking %d query samples for nearest neighbours."),
		NumPositions);

	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);
	TSharedRef<int32> BatchCounter             = MakeShared<int32>(0);
	TSharedRef<int32> TotalCandidatesProcessed = MakeShared<int32>(0);

	// Delegate called on the game thread after each batch of candidates is
	// processed.  Progressively merges results into the cache and pushes them
	// to Niagara so the user sees updates as they arrive.
	FOnSpatialHashBatchResult BatchCallback = FOnSpatialHashBatchResult::CreateLambda(
		[WeakThis, OnComplete, BatchCounter, TotalCandidatesProcessed, NumPositions]
		(const TArray<FSpatialHashQueryResult>& BatchResults, bool bIsFinalBatch,
		 int32 TotalCandidatesPhase1, int32 HandledQuerySamples)
		{
			ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
			if (!This)
			{
				return;
			}

			const int32 CurrentBatch = ++(*BatchCounter);

			// On the first batch, log Phase 1 summary to VRLogger so the user
			// can see how many query samples were handled and how many candidate
			// trajectories were identified.
			if (CurrentBatch == 1)
			{
				const int32 UnhandledSamples = NumPositions - HandledQuerySamples;

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
					TEXT("Phase 1 complete – %d/%d query samples handled, %d candidate trajectories found."),
					HandledQuerySamples, NumPositions, TotalCandidatesPhase1);

				if (UnhandledSamples > 0)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("WARNING: %d query sample(s) had no loaded hash table and were skipped – "
						     "check timestep coverage for gaps in the visualization."),
						UnhandledSamples);
				}
			}

			*TotalCandidatesProcessed += BatchResults.Num();

			This->AppendBatchResults(BatchResults);

			UE_LOG(LogTemp, Log, TEXT("QueryPositionsBatchedAsync: Batch %d – %d trajectories in results (bFinal=%d)"),
				CurrentBatch, BatchResults.Num(), bIsFinalBatch ? 1 : 0);

			This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
				TEXT("QueryPositionsBatchedAsync: Batch %d - %d new trajectories found"),
				CurrentBatch, BatchResults.Num());

			if (bIsFinalBatch)
			{
				UE_LOG(LogTemp, Log,
					TEXT("ATrajectoryQueryNiagaraActor: All batches complete – %d trajectories found in total."),
					This->CachedResults.Num());

				This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
					TEXT("All batches complete – %d trajectories found in total."),
					This->CachedResults.Num());

				// Sanity-check: verify that the number of accumulated result
				// trajectories does not exceed the Phase 1 candidate count.
				if (*TotalCandidatesProcessed > TotalCandidatesPhase1)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("WARNING: Result trajectory count (%d) exceeds Phase 1 candidate count (%d) – "
						     "possible duplicate handling."),
						*TotalCandidatesProcessed, TotalCandidatesPhase1);
				}
				else
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(
						TEXT("Check OK – %d of %d candidate trajectories produced results within the query radius."),
						*TotalCandidatesProcessed, TotalCandidatesPhase1);
				}

				OnComplete.ExecuteIfBound();
			}
		}
	);

	Manager->QueryPositionsBatchedAsync(
		DatasetDirectory,
		QueryPositions,
		QueryTimeSteps,
		OuterQueryRadius,
		CellSize,
		BatchSize,
		-1LL, // no trajectory to exclude
		MoveTemp(BatchCallback)
	);

	return true;
}

// ─── Result accumulation ──────────────────────────────────────────────────────

void ATrajectoryQueryNiagaraActor::AppendBatchResults(
	const TArray<FSpatialHashQueryResult>& BatchResults)
{
	if (BatchResults.IsEmpty())
	{
		return;
	}

	// Each candidate trajectory appears in at most one batch, so simple
	// appending is safe.  Skip any duplicate trajectory IDs that could arise
	// from rare hash-table overlap edge cases.
	for (const FSpatialHashQueryResult& Result : BatchResults)
	{
		if (CachedResultsIndex.Contains(Result.TrajectoryId))
		{
			continue; // already present from an earlier batch
		}
		CachedResultsIndex.Add(Result.TrajectoryId, CachedResults.Num());
		CachedResults.Add(Result);
	}

	// Incrementally expand the bounding box over the new samples.
	FBox Bounds(bBoundsValid ? FBox(ResultBoundsMin, ResultBoundsMax) : FBox(EForceInit::ForceInit));
	if (!bBoundsValid)
	{
		// Include all query positions in the bounding box on the first update.
		for (const FVector& QP : CachedQueryPoints)
		{
			Bounds += QP;
		}
	}
	for (const FSpatialHashQueryResult& Result : BatchResults)
	{
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			// Expand the bounding box using the corrected world-space position so
			// that particles in periodic images are accounted for correctly.
			Bounds += Sample.Position + DecodeVolumeIndexToOffset(Sample.VolumeIndex, CachedPeriodicExtent);
		}
	}
	if (Bounds.IsValid)
	{
		bBoundsValid    = true;
		ResultBoundsMin = Bounds.Min;
		ResultBoundsMax = Bounds.Max;
	}

	// Push the full accumulated result set to Niagara for a progressive visual
	// update.  Pass false so the Niagara system is not deactivated and restarted
	// on each batch, allowing the already-running emitter to pick up the updated
	// arrays on its next tick without discarding in-flight particles.
	TransferResultsToNiagara(CachedQueryPoints, CachedResults, true);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Batch appended – %d new trajectories, %d total, bounds [%s]–[%s]."),
		BatchResults.Num(), CachedResults.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::StoreQueryResults(
	const TArray<FVector>& QueryPoints,
	const TArray<FSpatialHashQueryResult>& Results)
{
	CachedQueryPoints = QueryPoints;
	CachedResults     = Results;

	// Compute bounding box over all query + result points.
	FBox Bounds(EForceInit::ForceInit);
	for (const FVector& QP : QueryPoints)
	{
		Bounds += QP;
	}
	for (const FSpatialHashQueryResult& Result : Results)
	{
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			// Use corrected world-space position (raw + volume offset) for the AABB.
			Bounds += Sample.Position + DecodeVolumeIndexToOffset(Sample.VolumeIndex, CachedPeriodicExtent);
		}
	}

	ResultBoundsMin = Bounds.IsValid ? Bounds.Min : FVector::ZeroVector;
	ResultBoundsMax = Bounds.IsValid ? Bounds.Max : FVector::ZeroVector;

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Results stored – %d trajectories, bounds [%s]–[%s]."),
		Results.Num(), *ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::TransferResultsToNiagara(
	const TArray<FVector>& QueryPoints,
	const TArray<FSpatialHashQueryResult>& Results,
	bool bReactivate)
{
	if (!NiagaraComponent)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: No NiagaraComponent available. "
			     "Assign a NiagaraSystem asset to this actor."));
		return;
	}

	// ── Build flat result arrays ──────────────────────────────────────────────

	// ResultPoints: all sample positions concatenated in trajectory order
	TArray<FVector> ResultPoints;

	// ResultDistances: distance from the query point for each sample (parallel to ResultPoints)
	TArray<float> ResultDistances;

	// ResultVolumeIndices: per-sample periodic volume index (parallel to ResultPoints).
	// 0 = original simulation box; non-zero = periodic image.
	// Decoded in HLSL via DecodeVolumeIndex / GetVolumeWorldOffset (see PERIODIC_VOLUME_INDEX.md).
	TArray<int32> ResultVolumeIndices;

	// Per-trajectory metadata arrays (one entry per result trajectory)
	TArray<int32> ResultTrajectoryIds;
	TArray<float> ResultMinDistances;
	TArray<int32> ResultMinDistanceTimeSteps;
	TArray<int32> ResultTrajStartIndex;
	TArray<int32> ResultStartTime;

	ResultTrajectoryIds.Reserve(Results.Num());
	ResultMinDistances.Reserve(Results.Num());
	ResultMinDistanceTimeSteps.Reserve(Results.Num());
	ResultTrajStartIndex.Reserve(Results.Num());
	ResultStartTime.Reserve(Results.Num());

	for (const FSpatialHashQueryResult& Result : Results)
	{
		ResultTrajectoryIds.Add(Result.TrajectoryId);
		float MinDistance = 0.0f;
		int32 MinDistanceTimeStep = INDEX_NONE;
		bool bHasMinDistance = false;
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			if (!bHasMinDistance || Sample.Distance < MinDistance)
			{
				MinDistance = Sample.Distance;
				MinDistanceTimeStep = Sample.TimeStep;
				bHasMinDistance = true;
			}
		}
		ResultMinDistances.Add(MinDistance);
		ResultMinDistanceTimeSteps.Add(MinDistanceTimeStep);
		ResultTrajStartIndex.Add(ResultPoints.Num());
		ResultStartTime.Add(Result.SamplePoints.Num() > 0 ? Result.SamplePoints[0].TimeStep : 0);

		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			ResultPoints.Add(Sample.Position);
			ResultDistances.Add(Sample.Distance);
			ResultVolumeIndices.Add(Sample.VolumeIndex);
		}
	}

	// ── Build query volume index array ───────────────────────────────────────
	//
	// Parallel to QueryPoints (which Niagara receives as raw/wrapped coordinates).
	// Each element encodes how many periodic box-lengths must be added to the raw
	// position to place it in the same continuous image as the unwrapped trajectory.
	//
	// When non-periodic, all indices are 0 (no offset needed).
	// When periodic, CachedQueryPointsRaw holds the original wrapped positions and
	// QueryPoints (the parameter) holds the unwrapped positions used for transforms.
	const TArray<FVector>& RawQueryPoints = CachedQueryPointsRaw.IsEmpty() ? QueryPoints : CachedQueryPointsRaw;

	TArray<int32> QueryVolumeIndices;
	QueryVolumeIndices.Reserve(RawQueryPoints.Num());
	if (!CachedPeriodicExtent.IsNearlyZero() && !CachedQueryPointsRaw.IsEmpty())
	{
		// CachedQueryPointsRaw and CachedQueryPoints (the unwrapped version passed as
		// QueryPoints) are always built together in FireAsyncQueriesWithCallback and
		// must have the same length.
		ensure(CachedQueryPointsRaw.Num() == QueryPoints.Num());

		for (int32 i = 0; i < RawQueryPoints.Num(); ++i)
		{
			const FVector& Raw       = RawQueryPoints[i];
			const FVector& Unwrapped = QueryPoints[i];
			// Compute the per-axis hop count: how many box-lengths to ADD to Raw
			// to reach the Unwrapped image (identical to ComputeVolumeIndex(Raw, Unwrapped)).
			const int32 ix = (CachedPeriodicExtent.X > 0.0f)
				? FMath::RoundToInt((Unwrapped.X - Raw.X) / CachedPeriodicExtent.X) : 0;
			const int32 iy = (CachedPeriodicExtent.Y > 0.0f)
				? FMath::RoundToInt((Unwrapped.Y - Raw.Y) / CachedPeriodicExtent.Y) : 0;
			const int32 iz = (CachedPeriodicExtent.Z > 0.0f)
				? FMath::RoundToInt((Unwrapped.Z - Raw.Z) / CachedPeriodicExtent.Z) : 0;
			QueryVolumeIndices.Add((ix & 0xFF) | ((iy & 0xFF) << 8) | ((iz & 0xFF) << 16));
		}
	}
	else
	{
		QueryVolumeIndices.Init(0, RawQueryPoints.Num());
	}

	// ── Build query-relative transform arrays ────────────────────────────────
	//
	// Transforms are computed from the UNWRAPPED query positions (the QueryPoints
	// parameter) so that translations and rotations are continuous across periodic
	// boundaries.  The shader reconstructs the unwrapped world position as:
	//   UnwrappedWorldPos[i] = RawQueryPoints[i] + GetVolumeWorldOffset(QueryVolumeIndices[i], PeriodicVolumeExtent)
	// which equals QueryPoints[i], so the transforms remain consistent.
	//
	// For each query sample point i:
	//   QueryTranslations[i] = QueryPoints[0] - QueryPoints[i]
	//   QueryRotations[i]    = rotation that aligns the forward vector at i to
	//                          the forward vector at 0.
	//
	// The forward vector at point i is the normalised direction from
	// QueryPoints[i] to QueryPoints[i+1].  For the last point the direction
	// from the penultimate point is reused.  When only one point is present
	// FVector::ForwardVector is used as a neutral fallback.

	TArray<FVector> QueryTranslations;
	TArray<FQuat>   QueryRotations;

	if (QueryPoints.Num() > 0)
	{
		QueryTranslations.Reserve(QueryPoints.Num());
		QueryRotations.Reserve(QueryPoints.Num());

		// Returns the normalised forward vector for sample index i.
		auto GetForwardAt = [&QueryPoints](int32 i) -> FVector
		{
			if (QueryPoints.Num() == 1)
			{
				return FVector::ForwardVector;
			}
			// Clamp so the last point reuses the direction of the penultimate one.
			const int32 From = FMath::Min(i,     QueryPoints.Num() - 2);
			const int32 To   = FMath::Min(i + 1, QueryPoints.Num() - 1);
			const FVector Dir = (QueryPoints[To] - QueryPoints[From]).GetSafeNormal();
			return Dir.IsNearlyZero() ? FVector::ForwardVector : Dir;
		};

		const FVector FirstForward = GetForwardAt(0);

		for (int32 i = 0; i < QueryPoints.Num(); ++i)
		{
			const FVector CurForward = GetForwardAt(i);
			QueryTranslations.Add(QueryPoints[0] - QueryPoints[i]);
			//if (i >0)
			//	UE_LOG(LogTemp, Log,
			//	TEXT("%d with %f"), i,  QueryTranslations[i].Length() - QueryTranslations[i-1].Length());

			QueryRotations.Add(FQuat::FindBetweenNormals(CurForward, FirstForward));
		}
	}

	// ── Transfer to Niagara user parameters ──────────────────────────────────

	// QueryPoints: raw (wrapped) simulation coordinates, consistent with ResultPoints.
	// The shader uses QueryVolumeIndices to reconstruct the correct world position.
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryPoints"), RawQueryPoints);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("ResultPoints"), ResultPoints);

	// Float array: per-sample distance from the query point (parallel to ResultPoints)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultDistances"), ResultDistances);

	// Query-relative transform arrays
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryTranslations"), QueryTranslations);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(
		NiagaraComponent, FName("QueryRotations"), QueryRotations);

	// Integer arrays (Int Array type in Niagara)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajectoryIds"), ResultTrajectoryIds);

	// Float array: minimum distance reached by each trajectory (parallel to ResultTrajectoryIds)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayFloat(
		NiagaraComponent, FName("ResultMinDistances"), ResultMinDistances);

	// Integer array: timestep where each trajectory reaches its minimum distance
	// (parallel to ResultTrajectoryIds / ResultMinDistances).
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultMinDistanceTimeSteps"), ResultMinDistanceTimeSteps);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajStartIndex"), ResultTrajStartIndex);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultStartTime"), ResultStartTime);

	// Per-sample volume index array (parallel to ResultPoints).
	// Encodes the periodic image each sample belongs to; 0 = original box.
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultVolumeIndices"), ResultVolumeIndices);

	// Per-query-point volume index array (parallel to QueryPoints).
	// Encodes how many box-lengths the raw query position must be shifted to
	// reach the unwrapped (continuous) trajectory image.  0 = original box.
	// QueryVolumeIndices[0] is always 0 (first point = reference / anchor).
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("QueryVolumeIndices"), QueryVolumeIndices);

	// Scalar user parameters
	NiagaraComponent->SetVariableFloat(FName("InnerRadius"), InnerRadius);
	NiagaraComponent->SetVariableFloat(FName("QueryRadius"), OuterQueryRadius);
	NiagaraComponent->SetVariableFloat(FName("OuterRadius"), OuterRadius);
	NiagaraComponent->SetVariableFloat(FName("CollisionRadius"), CollisionRadius);
	NiagaraComponent->SetVariableInt(FName("QueryTimeStart"), QueryTimeStart);
	NiagaraComponent->SetVariableInt(FName("QueryTimeEnd"), QueryTimeEnd);

	// Visualization range – default to the full query range on initial transfer
	NiagaraComponent->SetVariableInt(FName("VisTimeStart"), VisualizationTimeStart);
	NiagaraComponent->SetVariableInt(FName("VisTimeEnd"),   VisualizationTimeEnd);

	// Color encoding (0 = Timestep, 1 = Velocity)
	NiagaraComponent->SetVariableInt(FName("ColorEncoding"), static_cast<int32>(ColorEncoding));
	NiagaraComponent->SetVariableInt(FName("DistanceFilter"), static_cast<int32>(DistanceFilter));

	// Bounding box – use the stored values computed by StoreQueryResults
	NiagaraComponent->SetVariableVec3(FName("BoundsMin"), ResultBoundsMin);
	NiagaraComponent->SetVariableVec3(FName("BoundsMax"), ResultBoundsMax);

	// Periodic box size (world units per axis).  ZeroVector when non-periodic.
	// Used by the HLSL GetVolumeWorldOffset helper to compute per-sample position
	// offsets from the ResultVolumeIndices array.
	NiagaraComponent->SetVariableVec3(FName("PeriodicVolumeExtent"), CachedPeriodicExtent);

	// Activate the system now that all data has been pushed.
	if (bReactivate)
	{
		// removed old particles immediately
		NiagaraComponent->DeactivateImmediate();
		// NiagaraComponent->ResetSystem();
		NiagaraComponent->Activate(true);
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Niagara system updated – "
		     "%d query points, %d result points across %d trajectories. "
		     "Bounds: [%s] – [%s]."),
		QueryPoints.Num(), ResultPoints.Num(), Results.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}
