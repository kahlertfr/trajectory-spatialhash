// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Algo/BinarySearch.h"
#include "VRLogManager.h"


// Maximum number of async queries in-flight simultaneously.
// Keeps the async API from being overwhelmed while still making good use of
// the available worker threads.  Must be at least 1.
static constexpr int32 MaxConcurrentQueries = 1;

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

// ─── Protected helper ─────────────────────────────────────────────────────────

bool ATrajectoryQueryNiagaraActor::FireAsyncQueriesWithCallback(
	FSimpleDelegate OnComplete,
	FSimpleDelegate OnFailure)
{
	if (!InitializeManager())
	{
		OnFailure.ExecuteIfBound();
		return false;
	}

	if (QueryPositions.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("ATrajectoryQueryNiagaraActor: QueryPositions array is empty – nothing to query."));
		OnFailure.ExecuteIfBound();
		return false;
	}

	// ── Reset cached state so progressive updates start clean ────────────────
	// CachedQueryPoints is built progressively: a position is only added when
	// at least one of its timestep queries returns non-empty results.
	CachedQueryPoints.Empty();
	CachedResults.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPositionIndices.Empty();
	bBoundsValid = false;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;

	// ── Bounded-concurrency dispatch ─────────────────────────────────────────
	// Seed up to MaxConcurrentQueries parallel queries (positions 0..N-1).
	// As each query completes its callback atomically claims the next pending
	// position slot and fires a new query, keeping the pool full until every
	// position has been processed.  This prevents flooding the async API with
	// hundreds of simultaneous requests (which caused all callbacks to resolve
	// at the same—wrong—timestep) while still achieving real parallelism.
	// Callbacks arrive on the game thread, so shared state mutation is safe
	// without a mutex.

	const int32 NumPositions = QueryPositions.Num();
	const int32 TimeRangeSize = QueryTimeEnd - QueryTimeStart + 1;

	if (NumPositions > TimeRangeSize)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("ATrajectoryQueryNiagaraActor: QueryPositions.Num() (%d) exceeds the time range [%d,%d] (%d steps). "
			     "Positions beyond index %d map to timesteps outside the loaded range and will return empty results."),
			NumPositions, QueryTimeStart, QueryTimeEnd, TimeRangeSize, TimeRangeSize - 1);
	}

	// Seed workers take slots 0..InitialWorkers-1 statically.
	// NextIndex is pre-advanced to InitialWorkers so the first callback that
	// fires claims slot InitialWorkers, the second claims InitialWorkers+1, etc.
	const int32 InitialWorkers = FMath::Min(MaxConcurrentQueries, NumPositions);
	TSharedRef<FThreadSafeCounter> NextIndex    = MakeShared<FThreadSafeCounter>(InitialWorkers);
	TSharedRef<FThreadSafeCounter> PendingCount = MakeShared<FThreadSafeCounter>(NumPositions);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Seeding %d/%d concurrent queries (max %d), outer radius %.2f, t=[%d,%d]."),
		InitialWorkers, NumPositions, MaxConcurrentQueries, OuterQueryRadius, QueryTimeStart, QueryTimeEnd);

	GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("Seeding %d/%d concurrent queries (max %d), outer radius %.2f, t=[%d,%d]."),
		InitialWorkers, NumPositions, MaxConcurrentQueries, OuterQueryRadius, QueryTimeStart, QueryTimeEnd);

	for (int32 i = 0; i < InitialWorkers; ++i)
	{
		GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("ATrajectoryQueryNiagaraActor: Start query %i."),
			i);
		FireQueryForPosition(i, NextIndex, PendingCount, NumPositions, OnComplete);
	}

	return true;
}

void ATrajectoryQueryNiagaraActor::FireQueryForPosition(
	int32 PositionIndex,
	TSharedRef<FThreadSafeCounter> NextIndex,
	TSharedRef<FThreadSafeCounter> PendingCount,
	int32 NumPositions,
	FSimpleDelegate OnComplete)
{
	const FVector Position = QueryPositions[PositionIndex];
	// Each position corresponds to exactly one timestep in the queried range.
	// Clamp to QueryTimeEnd so queries are never fired outside the loaded range.
	const int32 TimeStep = FMath::Min(QueryTimeStart + PositionIndex, QueryTimeEnd);

	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);

	Manager->QueryRadiusWithDistanceCheckAsync(
		DatasetDirectory,
		Position,
		OuterQueryRadius,
		CellSize,
		TimeStep,
		FOnSpatialHashQueryComplete::CreateLambda(
			[WeakThis, PositionIndex, Position, NextIndex, PendingCount, NumPositions, OnComplete]
			(const TArray<FSpatialHashQueryResult>& Results)
			{
				ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
				if (!This)
				{
					return;
				}

				// Progressive update: incorporate this timestep's samples.
				This->AppendTimestepResults(Position, PositionIndex, Results);

				// Refuel the pool: atomically claim the next pending slot.
				// Increment() returns the new value; subtract 1 to get the slot.
				const int32 NextSlot = NextIndex->Increment() - 1;
				if (NextSlot < NumPositions)
				{
					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("ATrajectoryQueryNiagaraActor: Start query %i."),
						NextSlot);
					This->FireQueryForPosition(NextSlot, NextIndex, PendingCount, NumPositions, OnComplete);
				}

				// Fan-in: fire OnComplete when every position has been processed.
				// Decrement() returns the new value; 0 means we are the last.
				const int32 Remaining = PendingCount->Decrement();
				if (Remaining == 0)
				{
					UE_LOG(LogTemp, Log,
						TEXT("ATrajectoryQueryNiagaraActor: All %d queries complete – "
						     "%d positions with results, %d trajectories found in total."),
						NumPositions, This->CachedQueryPoints.Num(), This->CachedResults.Num());
					OnComplete.ExecuteIfBound();

					This->GetGameInstance()->GetSubsystem<UVRLogManager>()->AddMessageF(TEXT("ATrajectoryQueryNiagaraActor: All %d queries complete – "
						"%d positions with results, %d trajectories found in total."),
						NumPositions, This->CachedQueryPoints.Num(), This->CachedResults.Num());
				}
			}
		)
	);
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

void ATrajectoryQueryNiagaraActor::AppendTimestepResults(
	const FVector& QueryPosition,
	int32 PositionIndex,
	const TArray<FSpatialHashQueryResult>& Results)
{
	// Nothing to do if this position query returned no trajectories.
	// The query position is NOT added to CachedQueryPoints in this case.
	if (Results.IsEmpty())
	{
		return;
	}

	// If this is the first result for this query position, insert it into
	// CachedQueryPoints at the position that maintains the original QueryPositions
	// order (ascending by PositionIndex).
	if (Algo::BinarySearch(CachedQueryPositionIndices, PositionIndex) == INDEX_NONE)
	{
		// Find the insertion point in the sorted index list.
		const int32 InsertAt = Algo::LowerBound(CachedQueryPositionIndices, PositionIndex);
		CachedQueryPositionIndices.Insert(PositionIndex, InsertAt);
		// Mirror the insertion in CachedQueryPoints so both arrays stay in sync.
		CachedQueryPoints.Insert(QueryPosition, InsertAt);
	}

	// Merge each incoming result into CachedResults by trajectory ID.
	// Each element of Results is one trajectory found within the query radius
	// at the queried timestep.  For trajectories already in CachedResults
	// (found by an earlier position query), insert new samples at the correct
	// sorted TimeStep positions using binary search.
	for (const FSpatialHashQueryResult& NewResult : Results)
	{
		if (const int32* ExistingIdx = CachedResultsIndex.Find(NewResult.TrajectoryId))
		{
			// Trajectory already known: insert each new sample at the correct
			// sorted position to maintain ascending TimeStep order.
			TArray<FTrajectorySamplePoint>& Existing = CachedResults[*ExistingIdx].SamplePoints;
			for (const FTrajectorySamplePoint& NewSample : NewResult.SamplePoints)
			{
				// Binary search for the correct insertion position to maintain
				// ascending TimeStep order.
				const int32 InsertPos = Algo::LowerBoundBy(
					Existing, NewSample.TimeStep,
					[](const FTrajectorySamplePoint& S) { return S.TimeStep; });
				Existing.Insert(NewSample, InsertPos);
			}
		}
		else
		{
			// New trajectory: append and register its index for O(1) future lookup.
			CachedResultsIndex.Add(NewResult.TrajectoryId, CachedResults.Num());
			CachedResults.Add(NewResult);
		}
	}

	// Incrementally expand the bounding box.  Only positions that produced
	// results (tracked via CachedQueryPositionIndices) are included — along
	// with the sample positions from those results.
	FBox Bounds(bBoundsValid ? FBox(ResultBoundsMin, ResultBoundsMax) : FBox(EForceInit::ForceInit));
	Bounds += QueryPosition;
	for (const FSpatialHashQueryResult& Result : Results)
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

	// Push only the updated arrays – do not deactivate/reactivate the system.
	// The Niagara emitter polls the array data interfaces directly and will pick
	// up the new data on its next tick without needing a full system restart.
	TransferResultsToNiagara(CachedQueryPoints, CachedResults, true);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Progressive update (position %d) – %d query points, %d trajectories so far, bounds [%s]–[%s]."),
		PositionIndex, CachedQueryPoints.Num(), CachedResults.Num(), *ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
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
	// Skipped for progressive updates – the emitter polls the arrays itself.
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
