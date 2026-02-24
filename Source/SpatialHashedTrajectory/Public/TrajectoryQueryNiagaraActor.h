// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialHashTableManager.h"
#include "TrajectoryQueryNiagaraActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * Actor that runs a spatial hash trajectory query and transfers the results to a Niagara System.
 *
 * The following Niagara user parameters are populated:
 * - PositionArray  QueryPoints          – query sample positions (length = number of query points)
 * - PositionArray  ResultPoints         – result sample positions ordered by trajectory (length = all result samples)
 * - Int Array      ResultTrajectoryIds  – original trajectory ID per result trajectory
 * - Int Array      ResultTrajStartIndex – start index into ResultPoints for each result trajectory
 * - Int Array      ResultStartTime      – starting timestep for each result trajectory
 * - float          InnerQueryRadius
 * - float          OuterQueryRadius
 * - int            QueryTimeStart
 * - int            QueryTimeEnd
 */
UCLASS(BlueprintType, Blueprintable)
class SPATIALHASHEDTRAJECTORY_API ATrajectoryQueryNiagaraActor : public AActor
{
	GENERATED_BODY()

public:
	ATrajectoryQueryNiagaraActor();

	// ─── Query Settings ───────────────────────────────────────────────────────

	/** Path to the dataset directory containing shard files */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	FString DatasetDirectory;

	/** Cell size used when loading / querying spatial hash tables */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float CellSize = 10.0f;

	/** Inner query radius (nearest-neighbor inner band) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float InnerQueryRadius = 25.0f;

	/** Outer query radius */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	float OuterQueryRadius = 50.0f;

	/** First time step of the query range (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	int32 QueryTimeStart = 0;

	/** Last time step of the query range (inclusive) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	int32 QueryTimeEnd = 100;

	/**
	 * Positions used as query centres.
	 * One async radius-over-time-range query is fired per entry.
	 * Results from all positions are merged before being transferred to Niagara.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	TArray<FVector> QueryPositions;

	// ─── Niagara Settings ─────────────────────────────────────────────────────

	/** Niagara System asset to spawn / configure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraSystem;

	// ─── Blueprint callable entry points ─────────────────────────────────────

	/**
	 * Run the spatial hash query and push the results to the attached Niagara component.
	 * Call this after the actor has been placed and configured in the level.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization")
	void RunQueryAndUpdateNiagara();

protected:
	virtual void BeginPlay() override;

	/** Niagara component that hosts the effect (read-only from Blueprint subclasses) */
	UPROPERTY(BlueprintReadOnly, Category = "Niagara")
	UNiagaraComponent* NiagaraComponent;

	/** Spatial hash table manager used for queries */
	UPROPERTY()
	USpatialHashTableManager* Manager;

	/** Initialize the spatial hash table manager and load required hash tables */
	bool InitializeManager();

private:
	/**
	 * Convert query results to flat arrays suitable for Niagara and transfer them
	 * to the Niagara component's user parameters.
	 *
	 * @param QueryPoints     Positions that were used as query inputs
	 * @param Results         Query result trajectories with sample points
	 */
	void TransferResultsToNiagara(
		const TArray<FVector>& QueryPoints,
		const TArray<FSpatialHashQueryResult>& Results);
};
