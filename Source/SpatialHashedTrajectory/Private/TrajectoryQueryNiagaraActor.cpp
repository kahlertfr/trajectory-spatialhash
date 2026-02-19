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

	// Automatically run the query when the actor starts playing.
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

void ATrajectoryQueryNiagaraActor::RunQueryAndUpdateNiagara()
{
	if (!InitializeManager())
	{
		return;
	}

	TArray<FSpatialHashQueryResult> Results;
	TArray<FVector> QueryPoints;

	if (QueryTrajectoryId >= 0)
	{
		// Case C: query trajectory over time range
		const int32 NumFound = Manager->QueryTrajectoryRadiusOverTimeRange(
			DatasetDirectory,
			QueryTrajectoryId,
			OuterQueryRadius,
			CellSize,
			QueryTimeStart,
			QueryTimeEnd,
			Results
		);

		UE_LOG(LogTemp, Log,
			TEXT("ATrajectoryQueryNiagaraActor: Case C query found %d trajectories near trajectory %d."),
			NumFound, QueryTrajectoryId);

		// The query points are the sample positions of the query trajectory itself.
		// We collect them from the result whose TrajectoryId matches QueryTrajectoryId,
		// if present, otherwise fall back to the actor location as a single point.
		bool bFoundQueryTraj = false;
		for (const FSpatialHashQueryResult& R : Results)
		{
			if (R.TrajectoryId == QueryTrajectoryId)
			{
				for (const FTrajectorySamplePoint& S : R.SamplePoints)
				{
					QueryPoints.Add(S.Position);
				}
				bFoundQueryTraj = true;
				break;
			}
		}

		if (!bFoundQueryTraj)
		{
			QueryPoints.Add(GetActorLocation());
		}
	}
	else
	{
		// Case B: single fixed position over time range
		const FVector QueryPosition = GetActorLocation();
		QueryPoints.Add(QueryPosition);

		const int32 NumFound = Manager->QueryRadiusOverTimeRange(
			DatasetDirectory,
			QueryPosition,
			OuterQueryRadius,
			CellSize,
			QueryTimeStart,
			QueryTimeEnd,
			Results
		);

		UE_LOG(LogTemp, Log,
			TEXT("ATrajectoryQueryNiagaraActor: Case B query found %d trajectories at position %s."),
			NumFound, *QueryPosition.ToString());
	}

	TransferResultsToNiagara(QueryPoints, Results);
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

	// Activate the system now that all data has been pushed
	NiagaraComponent->Activate(true);

	UE_LOG(LogTemp, Log,
		TEXT("ATrajectoryQueryNiagaraActor: Niagara system updated – "
		     "%d query points, %d result points across %d trajectories."),
		QueryPoints.Num(), ResultPoints.Num(), Results.Num());
}
