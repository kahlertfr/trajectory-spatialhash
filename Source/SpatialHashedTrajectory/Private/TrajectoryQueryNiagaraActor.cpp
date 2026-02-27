// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrajectoryQueryNiagaraActor.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"

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

	// Automatically run the query and update Niagara when the actor starts playing.
	if (!DatasetDirectory.IsEmpty())
	{
		RunQueryAndUpdateNiagara();
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
	CachedQueryPoints  = QueryPositions;   // all positions are known up-front
	CachedResults.Empty();

	// Pre-compute bounds over all query positions (they are known up-front).
	// AppendPartialResults will only expand these incrementally with new results.
	{
		FBox QBounds(EForceInit::ForceInit);
		for (const FVector& QP : CachedQueryPoints)
		{
			QBounds += QP;
		}
		ResultBoundsMin = QBounds.IsValid ? QBounds.Min : FVector::ZeroVector;
		ResultBoundsMax = QBounds.IsValid ? QBounds.Max : FVector::ZeroVector;
	}

	// ── Fan-out: one async query per query position ───────────────────────────
	// Progressive update: each per-position callback immediately appends its
	// results and refreshes the Niagara system so the user sees them as they
	// arrive, without waiting for all queries to finish.
	// FOnSpatialHashQueryComplete callbacks are delivered on the game thread,
	// so CachedResults.Append is safe without a mutex.
	// FThreadSafeCounter is used for the decrement in case the API ever delivers
	// callbacks from a worker thread in the future.

	const int32 NumQueries = QueryPositions.Num();
	TSharedRef<FThreadSafeCounter> PendingCount = MakeShared<FThreadSafeCounter>(NumQueries);

	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> WeakThis(this);

	for (const FVector& Position : QueryPositions)
	{
		Manager->QueryRadiusOverTimeRangeAsync(
			DatasetDirectory,
			Position,
			OuterQueryRadius,
			CellSize,
			QueryTimeStart,
			QueryTimeEnd,
			FOnSpatialHashQueryComplete::CreateLambda(
				[WeakThis, PendingCount, OnComplete]
				(const TArray<FSpatialHashQueryResult>& Results)
				{
					ATrajectoryQueryNiagaraActor* This = WeakThis.Get();
					if (!This)
					{
						return;
					}

					// Progressive update: push this query's results to Niagara immediately.
					This->AppendPartialResults(Results);

					// Fan-in: when all per-position queries have completed,
					// invoke the completion callback.
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
		TEXT("ATrajectoryQueryNiagaraActor: Fired %d async queries (outer radius %.2f, t=[%d,%d])."),
		NumQueries, OuterQueryRadius, QueryTimeStart, QueryTimeEnd);

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

void ATrajectoryQueryNiagaraActor::AppendPartialResults(
	const TArray<FSpatialHashQueryResult>& NewResults)
{
	// Extend the accumulated result set.
	CachedResults.Append(NewResults);

	// Incrementally expand the bounding box with only the new results.
	// ResultBoundsMin/Max were pre-seeded with the query positions in
	// FireAsyncQueriesWithCallback, so we only need to process NewResults here.
	FBox Bounds(ResultBoundsMin, ResultBoundsMax);
	for (const FSpatialHashQueryResult& Result : NewResults)
	{
		for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
		{
			Bounds += Sample.Position;
		}
	}
	ResultBoundsMin = Bounds.Min;
	ResultBoundsMax = Bounds.Max;

	// Deactivate the system so the next Activate() call performs a clean reset.
	if (NiagaraComponent)
	{
		NiagaraComponent->Deactivate();
	}

	// Push the enlarged dataset and reactivate Niagara immediately so the user
	// sees results progressively as each per-position query completes.
	TransferResultsToNiagara(CachedQueryPoints, CachedResults);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Progressive update – %d trajectories so far, bounds [%s]–[%s]."),
		CachedResults.Num(), *ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}

void ATrajectoryQueryNiagaraActor::TransferResultsToNiagara(
	const TArray<FVector>& QueryPoints,
	const TArray<FSpatialHashQueryResult>& Results)
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

	// Activate the system now that all data has been pushed
	NiagaraComponent->Activate(true);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Niagara system updated – "
		     "%d query points, %d result points across %d trajectories. "
		     "Bounds: [%s] – [%s]."),
		QueryPoints.Num(), ResultPoints.Num(), Results.Num(),
		*ResultBoundsMin.ToString(), *ResultBoundsMax.ToString());
}
