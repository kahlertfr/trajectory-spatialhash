// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpatialHashTableManager.h"
#include "TrajectoryQueryNiagaraActor.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;

/**
 * Color encoding mode used by the Niagara shader to colorize trajectory particles.
 * The integer value is passed to the Niagara user parameter "ColorEncoding".
 */
UENUM(BlueprintType)
enum class ETrajectoryColorEncoding : uint8
{
	/** Color mapped from the normalized timestep along the trajectory */
	Timestep UMETA(DisplayName = "Timestep"),

	/** Color mapped from the particle velocity (speed) */
	Velocity UMETA(DisplayName = "Velocity"),
};

/**
 * Actor that runs a spatial hash trajectory query and transfers the results to a Niagara System.
 *
 * The following Niagara user parameters are populated when data is transferred:
 * - PositionArray  QueryPoints          – query sample positions (length = number of query points)
 * - PositionArray  ResultPoints         – result sample positions ordered by trajectory (length = all result samples)
 * - PositionArray  QueryTranslations    – per query point: translation from that point to the first query point (QueryPoints[0] - QueryPoints[i])
 * - QuatArray      QueryRotations       – per query point: rotation that aligns its forward vector to the first query point's forward vector
 * - Int Array      ResultTrajectoryIds  – original trajectory ID per result trajectory
 * - Int Array      ResultTrajStartIndex – start index into ResultPoints for each result trajectory
 * - Int Array      ResultStartTime      – starting timestep for each result trajectory
 * - float          InnerQueryRadius
 * - float          OuterQueryRadius
 * - int            QueryTimeStart
 * - int            QueryTimeEnd
 * - Vector         BoundsMin            – minimum corner of the AABB enclosing all query + result points
 * - Vector         BoundsMax            – maximum corner of the AABB enclosing all query + result points
 *
 * The following Niagara user parameters can be updated at runtime without re-transferring data
 * (via SetVisualizationTimeRange / SetColorEncoding):
 * - int            VisTimeStart         – first timestep shown by the shader (inclusive)
 * - int            VisTimeEnd           – last timestep shown by the shader (inclusive)
 * - int            ColorEncoding        – 0 = Timestep, 1 = Velocity (see ETrajectoryColorEncoding)
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
	 * QueryPositions[i] is queried at timestep min(QueryTimeStart + i, QueryTimeEnd).
	 * All positions are queried in parallel in the first phase of the pipeline,
	 * and results are processed in batches of BatchSize trajectories to allow
	 * progressive updates to the Niagara system.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	TArray<FVector> QueryPositions;

	/**
	 * Maximum number of candidate trajectories processed per batch.
	 * After all query positions have been searched in parallel, the found
	 * candidate trajectories are split into batches of this size.  Each batch
	 * loads its data and filters it before updating Niagara, providing
	 * progressive visual feedback.  A value of 10000 is a reasonable default.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings",
		meta = (ClampMin = "1", UIMin = "100"))
	int32 BatchSize = 10000;

	/**
	 * Periodic boundary conditions for the dataset.
	 *
	 * Enable this when the trajectory dataset was generated inside a periodic
	 * simulation box (e.g. a molecular dynamics simulation).  The manager will
	 * then wrap cell lookups at the boundary, apply the minimum-image distance
	 * convention, and shift reported neighbour positions to the image closest
	 * to the (unwrapped) query trajectory so that visualised paths are
	 * continuous even when they cross a periodic boundary.
	 *
	 * Set bIsPeriodic to true and leave Extent as ZeroVector to infer the box
	 * size automatically from the loaded hash tables.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Query Settings")
	FPeriodicVolume PeriodicVolume;

	// ─── Niagara Settings ─────────────────────────────────────────────────────

	/** Niagara System asset to spawn / configure */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Niagara")
	UNiagaraSystem* NiagaraSystem;

	// ─── Visualization Settings ───────────────────────────────────────────────

	/**
	 * First timestep shown by the shader (inclusive).
	 * Initialized to match QueryTimeStart.  Can be changed at runtime via
	 * SetVisualizationTimeRange without re-querying or re-transferring the
	 * trajectory data.  The value is preserved across queries so a custom
	 * range set by the user survives a new RunQuery call.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 VisualizationTimeStart = 0;

	/**
	 * Last timestep shown by the shader (inclusive).
	 * Initialized to match QueryTimeEnd.  Can be changed at runtime via
	 * SetVisualizationTimeRange without re-querying or re-transferring the
	 * trajectory data.  The value is preserved across queries so a custom
	 * range set by the user survives a new RunQuery call.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	int32 VisualizationTimeEnd = 100;

	/**
	 * Color encoding mode used by the Niagara shader.
	 * Can be changed at runtime via SetColorEncoding without re-transferring data.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visualization")
	ETrajectoryColorEncoding ColorEncoding = ETrajectoryColorEncoding::Timestep;

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
	 * Update the visible time range on the active Niagara system without re-transferring
	 * trajectory data.  Writes the Niagara user parameters "VisTimeStart" and "VisTimeEnd"
	 * so that the shader can immediately restrict which timesteps are rendered.
	 *
	 * Call this any time after the Niagara component has been activated (i.e. after
	 * TransferDataToNiagara / RunQueryAndUpdateNiagara has completed).
	 *
	 * @param NewTimeStart  First timestep to show (inclusive).
	 * @param NewTimeEnd    Last  timestep to show (inclusive).
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization")
	void SetVisualizationTimeRange(int32 NewTimeStart, int32 NewTimeEnd);

	/**
	 * Change the color encoding used by the Niagara shader without re-transferring
	 * trajectory data.  Writes the Niagara user parameter "ColorEncoding" (int) so
	 * that the shader can immediately switch its coloring scheme.
	 *
	 * Call this any time after the Niagara component has been activated.
	 *
	 * @param NewColorEncoding  The desired color encoding mode.
	 */
	UFUNCTION(BlueprintCallable, Category = "Trajectory Visualization")
	void SetColorEncoding(ETrajectoryColorEncoding NewColorEncoding);

	/**
	 * High-performance parallel/batched async query dispatch.
	 *
	 * Phase 1 – all query positions are searched simultaneously on worker threads
	 * to collect candidate trajectory IDs and their earliest/latest timesteps.
	 * Phase 2 – candidates are processed in batches of BatchSize; each batch
	 * loads shard data in parallel, filters against the query radius in parallel,
	 * and pushes the results to Niagara for progressive visual updates.
	 *
	 * Calls OnComplete when all batches have been processed, or OnFailure if a
	 * startup condition is not met (empty DatasetDirectory, empty QueryPositions).
	 * Returns true if the pipeline was successfully started.
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
	 * All query positions (mirrors QueryPositions).  Set at the start of each
	 * FireAsyncQueriesWithCallback call so Niagara always sees the full query
	 * trajectory for transform computations.
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
	 * Kept in sync with CachedResults so AppendBatchResults can detect
	 * (and skip) any duplicate trajectory IDs in O(1).
	 */
	TMap<int32, int32> CachedResultsIndex;

	/**
	 * Sorted list of QueryPositions indices that have produced at least one result.
	 * Retained for API compatibility; not actively populated in the new batched pipeline.
	 */
	TArray<int32> CachedQueryPositionIndices;

	/** True once the first result has been incorporated into ResultBoundsMin/Max. */
	bool bBoundsValid = false;

	/**
	 * Store completed query results and compute the result bounding box.
	 * Called from the completion callback on the game thread.
	 */
	void StoreQueryResults(
		const TArray<FVector>& QueryPoints,
		const TArray<FSpatialHashQueryResult>& Results);

	/**
	 * Append a batch of trajectory results delivered by QueryPositionsBatchedAsync.
	 * Trajectories already present in CachedResults (from an earlier batch) are
	 * skipped.  After merging, the bounding box is expanded and Niagara is updated
	 * with the full accumulated result set for a progressive visual update.
	 *
	 * Must be called on the game thread.
	 */
	void AppendBatchResults(const TArray<FSpatialHashQueryResult>& BatchResults);

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
