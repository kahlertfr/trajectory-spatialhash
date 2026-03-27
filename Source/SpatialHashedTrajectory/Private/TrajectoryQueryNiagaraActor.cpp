// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "VRLogManager.h"


ATrajectoryQueryNiagaraActor::ATrajectoryQueryNiagaraActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Manager = nullptr;
	NiagaraComponent = nullptr;
	NiagaraSystem = nullptr;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;
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

	if (QueryPositions.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ATrajectoryQueryNiagaraActor: QueryPositions array is empty – nothing to query."));
		OnFailure.ExecuteIfBound();
		return false;
	}

	// ── Reset cached state so progressive updates start clean ─────────────────
	// CachedQueryPoints is set to the full query trajectory up-front so that
	// Niagara receives the correct query-point transforms from the very first
	// progressive update.
	CachedQueryPoints              = QueryPositions;
	CachedResults.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPositionIndices.Empty();
	bBoundsValid    = false;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;

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
			Bounds += Sample.Position;
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
			Bounds += Sample.Position;
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

	// Per-trajectory metadata arrays (one entry per result trajectory)
	TArray<int32> ResultTrajectoryIds;
	TArray<int32> ResultTrajStartIndex;
	TArray<int32> ResultStartTime;

	ResultTrajectoryIds.Reserve(Results.Num());
	ResultTrajStartIndex.Reserve(Results.Num());
	ResultStartTime.Reserve(Results.Num());

	for (const FSpatialHashQueryResult& Result : Results)
	{
		ResultTrajectoryIds.Add(Result.TrajectoryId);
		ResultTrajStartIndex.Add(ResultPoints.Num());
		ResultStartTime.Add(Result.SamplePoints.Num() > 0 ? Result.SamplePoints[0].TimeStep : 0);

		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			ResultPoints.Add(Sample.Position);
		}
	}

	// ── Build query-relative transform arrays ────────────────────────────────
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
			if (i >0)
				UE_LOG(LogTemp, Log,
				TEXT("%d with %f"), i,  QueryTranslations[i].Length() - QueryTranslations[i-1].Length());

			QueryRotations.Add(FQuat::FindBetweenNormals(CurForward, FirstForward));
		}
	}

	// ── Transfer to Niagara user parameters ──────────────────────────────────

	// Position arrays (PositionArray type in Niagara)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryPoints"), QueryPoints);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("ResultPoints"), ResultPoints);

	// Query-relative transform arrays
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryTranslations"), QueryTranslations);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayQuat(
		NiagaraComponent, FName("QueryRotations"), QueryRotations);

	// Integer arrays (Int Array type in Niagara)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajectoryIds"), ResultTrajectoryIds);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultTrajStartIndex"), ResultTrajStartIndex);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayInt32(
		NiagaraComponent, FName("ResultStartTime"), ResultStartTime);

	// Scalar user parameters
	NiagaraComponent->SetVariableFloat(FName("InnerQueryRadius"), InnerQueryRadius);
	NiagaraComponent->SetVariableFloat(FName("OuterQueryRadius"), OuterQueryRadius);
	NiagaraComponent->SetVariableInt(FName("QueryTimeStart"), QueryTimeStart);
	NiagaraComponent->SetVariableInt(FName("QueryTimeEnd"), QueryTimeEnd);

	// Bounding box – use the stored values computed by StoreQueryResults
	NiagaraComponent->SetVariableVec3(FName("BoundsMin"), ResultBoundsMin);
	NiagaraComponent->SetVariableVec3(FName("BoundsMax"), ResultBoundsMax);

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

