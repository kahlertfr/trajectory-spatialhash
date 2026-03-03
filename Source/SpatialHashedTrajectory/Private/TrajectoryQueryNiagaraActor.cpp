// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "Algo/BinarySearch.h"

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

	const int32 LoadedCount = Manager->LoadHashTables(
		DatasetDirectory,
		CellSize,
		QueryTimeStart,
		QueryTimeEnd,
		true  // auto-create if missing
	);

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

	// ── Reset cached state so progressive updates start clean ─────────────────
	// CachedQueryPoints is built progressively: a position is only added when
	// at least one of its timestep queries returns non-empty results.
	CachedQueryPoints.Empty();
	CachedResults.Empty();
	CachedResultsIndex.Empty();
	CachedQueryPositionIndices.Empty();
	bBoundsValid = false;
	ResultBoundsMin = FVector::ZeroVector;
	ResultBoundsMax = FVector::ZeroVector;

	// ── Fan-out: one async query per (query position × timestep) ─────────────
	// Each query covers a single timestep so callbacks deliver exactly the
	// sample points for that timestep.  Results arrive and are merged
	// progressively; the emitter picks up updated arrays on its next tick.
	// Callbacks are documented to arrive on the game thread, so shared state
	// mutation is safe without a mutex.

	const int32 NumTimeSteps = QueryTimeEnd - QueryTimeStart + 1;
	const int32 NumQueries   = QueryPositions.Num();
	TSharedRef<FThreadSafeCounter> PendingCount = MakeShared<FThreadSafeCounter>(NumQueries);

	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);

	for (int32 PositionIndex = 0; PositionIndex < QueryPositions.Num(); ++PositionIndex)
	{
		const FVector Position = QueryPositions[PositionIndex];
		int32 TimeStep = QueryTimeStart + PositionIndex;
			Manager->QueryRadiusWithDistanceCheckAsync(
				DatasetDirectory,
				Position,
				OuterQueryRadius,
				CellSize,
				TimeStep,
				FOnSpatialHashQueryComplete::CreateLambda(
					[WeakThis, PendingCount, OnComplete, Position, PositionIndex, TimeStep]
					(const TArray<FSpatialHashQueryResult>& Results)
					{
						ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
						if (!This)
						{
							return;
						}

						// Progressive update: incorporate this timestep's samples
						// at the correct sorted position in each trajectory.
						// The query position is only added to CachedQueryPoints
						// when Results is non-empty.
						This->AppendTimestepResults(Position, PositionIndex, TimeStep, Results);

						// Fan-in: when all queries have completed, fire OnComplete.
						const int32 Remaining = PendingCount->Decrement();
						if (Remaining == 0)
						{
							UE_LOG(LogTemp, Log,
								TEXT("ATrajectoryQueryNiagaraActor: All %d async queries complete – "
								     "%d trajectories found in total."),
								This->CachedQueryPoints.Num(), This->CachedResults.Num());

							OnComplete.ExecuteIfBound();
						}
					}
				)
			);
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Fired %d async queries (%d positions, outer radius %.2f, t=[%d,%d])."),
		NumQueries, QueryPositions.Num(), OuterQueryRadius, QueryTimeStart, QueryTimeEnd);

	return true;
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
	int32 TimeStep,
	const TArray<FSpatialHashQueryResult>& Results)
{
	// Nothing to do if this timestep query returned no trajectories.
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
	// Each element of Results corresponds to one trajectory found within the
	// query radius at TimeStep.  Because this is a single-timestep query each
	// result carries exactly one sample point (at TimeStep).
	// For trajectories already in CachedResults (seen at another timestep
	// or from a different query position), insert the new sample at the
	// correct sorted position using binary search.
	for (const FSpatialHashQueryResult& NewResult : Results)
	{
		if (const int32* ExistingIdx = CachedResultsIndex.Find(NewResult.TrajectoryId))
		{
			// Trajectory already known: insert the new sample at the correct
			// sorted TimeStep position.  A single-timestep query yields exactly
			// one sample per trajectory result, so this is a single-element
			// insert per callback — O(log N) search + O(N) shift, where N is
			// the number of already-accumulated timesteps for this trajectory.
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

	TransferResultsToNiagara(CachedQueryPoints, CachedResults, true);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Progressive update (t=%d) – %d query points, %d trajectories so far, bounds [%s]–[%s]."),
		TimeStep, CachedQueryPoints.Num(), CachedResults.Num(), *ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
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

	// ── Transfer to Niagara user parameters ──────────────────────────────────

	// Position arrays (PositionArray type in Niagara)
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("QueryPoints"), QueryPoints);

	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector(
		NiagaraComponent, FName("ResultPoints"), ResultPoints);

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
		NiagaraComponent->Activate(true);
	}

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Niagara system updated – "
		     "%d query points, %d result points across %d trajectories. "
		     "Bounds: [%s] – [%s]."),
		QueryPoints.Num(), ResultPoints.Num(), Results.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}
