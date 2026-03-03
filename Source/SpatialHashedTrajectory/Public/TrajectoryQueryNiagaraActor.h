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
 * - Vector         BoundsMin            – minimum corner of the AABB enclosing all query + result points
 * - Vector         BoundsMax            – maximum corner of the AABB enclosing all query + result points
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
	 * One async single-timestep radius query is fired per (position × timestep) pair,
	 * so results arrive progressively as each timestep query completes.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	TArray<FVector> QueryPositions;

	// ─── Niagara Settings ─────────────────────────────────────────────────────

	/** Niagara System asset to spawn / configure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraSystem;

	// ─── Blueprint callable entry points ─────────────────────────────────────

	/**
	 * Push the cached query results to the attached Niagara component.
	 * Call this after RunQuery (async) has completed (i.e. after OnSuccess fires).
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization")
	void TransferDataToNiagara();

	/**
	 * Convenience wrapper: runs the query and transfers results to Niagara as soon as
	 * all async callbacks have fired. Equivalent to the async RunQuery node followed by
	 * TransferDataToNiagara() on the success pin.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization")
	void RunQueryAndUpdateNiagara();

	/**
	 * Core fan-out / fan-in async query dispatch.
	 * Stores results into CachedQueryPoints / CachedResults / ResultBoundsMin / ResultBoundsMax.
	 * Calls OnComplete when all queries finish successfully, or OnFailure if startup fails.
	 * Returns true if queries were started, false if a startup condition was not met.
	 * Accessible from URunTrajectoryQueryAsyncAction to inject its own callbacks.
	 */
	bool FireAsyncQueriesWithCallback(FSimpleDelegate OnComplete, FSimpleDelegate OnFailure = FSimpleDelegate());

protected:
	virtual void BeginPlay() override;

	/** Niagara component that hosts the effect (read-only from Blueprint subclasses) */
	UPROPERTY(BlueprintReadOnly, Category = "Niagara")
	UNiagaraComponent* NiagaraComponent;

	/** Spatial hash table manager used for queries */
	UPROPERTY()
	USpatialHashTableManager* Manager;

	/**
	 * Query positions for which at least one result has been received.
	 * Populated progressively during FireAsyncQueriesWithCallback; maintains
	 * the original QueryPositions order.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Query Results")
	TArray<FVector> CachedQueryPoints;

	/** Minimum corner of the AABB enclosing all query + result points from the last query */
	UPROPERTY(BlueprintReadOnly, Category = "Query Results")
	FVector ResultBoundsMin;

	/** Maximum corner of the AABB enclosing all query + result points from the last query */
	UPROPERTY(BlueprintReadOnly, Category = "Query Results")
	FVector ResultBoundsMax;

	/** Initialize the spatial hash table manager and load required hash tables */
	bool InitializeManager();

private:
	/** Results cached by the last completed query */
	TArray<FSpatialHashQueryResult> CachedResults;

	/**
	 * Lookup table: TrajectoryId → index into CachedResults.
	 * Kept in sync with CachedResults during FireAsyncQueriesWithCallback
	 * so AppendTimestepResults can locate existing trajectories in O(1).
	 */
	TMap<int32, int32> CachedResultsIndex;

	/**
	 * Sorted list of QueryPositions indices that have produced at least one result.
	 * Maintained in ascending index order so that CachedQueryPoints can be updated
	 * with a single sorted insertion rather than a full rebuild.
	 */
	TArray<int32> CachedQueryPositionIndices;

	/** True once the first result has been incorporated into ResultBoundsMin/Max. */
	bool bBoundsValid = false;

	/**
	 * Store completed query results and compute the result bounding box.
	 * Called from the fan-in callback on the game thread.
	 */
	void StoreQueryResults(
		const TArray<FVector>& QueryPoints,
		const TArray<FSpatialHashQueryResult>& Results);

	/**
	 * Incorporate the results from a single per-position async time-range query
	 * into the accumulated cache, then push the updated arrays to Niagara.
	 *
	 * Each element of Results may carry sample points across all queried timesteps.
	 * When a trajectory is already present in CachedResults (found by an earlier
	 * position query) new samples are inserted at the correct sorted positions
	 * using binary search — no duplicate-timestep check is required because
	 * each (position × timestep) pair is queried only once.
	 *
	 * Called on the game thread after each individual async query completes.
	 */
	void AppendTimestepResults(
		const FVector& QueryPosition,
		int32 PositionIndex,
		const TArray<FSpatialHashQueryResult>& Results);

	/**
	 * Push the supplied arrays to the Niagara component user parameters.
	 * Low-level implementation used by TransferDataToNiagara().
	 * @param bReactivate  When true (default) the component is activated after the transfer.
	 *                     Pass false for progressive updates where the emitter polls the arrays itself.
	 */
	void TransferResultsToNiagara(
		const TArray<FVector>& QueryPoints,
		const TArray<FSpatialHashQueryResult>& Results,
		bool bReactivate = true);
};
