// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpatialHashTable.h"
#include "SpatialHashTableBuilder.h"
#include "SpatialHashTableManager.generated.h"

// Forward declare callback delegate types for async queries
DECLARE_DELEGATE_OneParam(FOnSpatialHashQueryComplete, const TArray<FSpatialHashQueryResult>&);
DECLARE_DELEGATE_TwoParams(FOnSpatialHashDualQueryComplete, const TArray<FSpatialHashQueryResult>&, const TArray<FSpatialHashQueryResult>&);

/**
 * Result structure for nearest neighbor queries
 */
USTRUCT(BlueprintType)
struct FSpatialQueryResult
{
	GENERATED_BODY()

	/** Trajectory ID */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	int32 TrajectoryId;

	/** Distance from query position (only valid for radius queries) */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	float Distance;

	FSpatialQueryResult()
		: TrajectoryId(0)
		, Distance(0.0f)
	{
	}

	FSpatialQueryResult(int32 InId, float InDistance)
		: TrajectoryId(InId)
		, Distance(InDistance)
	{
	}
};

/**
 * Trajectory sample with position and time information
 */
USTRUCT(BlueprintType)
struct FTrajectorySamplePoint
{
	GENERATED_BODY()

	/** World position of the sample */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	FVector Position;

	/** Time step of this sample */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	int32 TimeStep;

	/** Distance from query point (only valid for radius queries) */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	float Distance;

	FTrajectorySamplePoint()
		: Position(FVector::ZeroVector)
		, TimeStep(0)
		, Distance(0.0f)
	{
	}

	FTrajectorySamplePoint(const FVector& InPosition, int32 InTimeStep, float InDistance)
		: Position(InPosition)
		, TimeStep(InTimeStep)
		, Distance(InDistance)
	{
	}
};

/**
 * Result structure containing full trajectory with all sample points
 */
USTRUCT(BlueprintType)
struct FSpatialHashQueryResult
{
	GENERATED_BODY()

	/** Trajectory ID */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	int32 TrajectoryId;

	/** All sample points for this trajectory within the query parameters */
	UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
	TArray<FTrajectorySamplePoint> SamplePoints;

	FSpatialHashQueryResult()
		: TrajectoryId(0)
	{
	}

	FSpatialHashQueryResult(int32 InId)
		: TrajectoryId(InId)
	{
	}
};

/**
 * Spatial Hash Table Manager
 * 
 * Blueprint-accessible manager for loading, creating, and querying spatial hash tables.
 * This class manages multiple hash tables with different cell sizes and time steps,
 * enabling efficient fixed-radius nearest neighbor searches on trajectory data.
 */
UCLASS(BlueprintType, Blueprintable)
class SPATIALHASHEDTRAJECTORY_API USpatialHashTableManager : public UObject
{
	GENERATED_BODY()

public:
	USpatialHashTableManager();

	/**
	 * Load hash tables from disk for a specific cell size
	 * If hash tables don't exist, attempts to create them from trajectory data
	 * 
	 * @param DatasetDirectory Base directory containing the dataset
	 * @param CellSize Cell size to load hash tables for
	 * @param StartTimeStep First time step to load (inclusive)
	 * @param EndTimeStep Last time step to load (inclusive)
	 * @param bAutoCreate If true, automatically creates hash tables if they don't exist (default: true)
	 * @return Number of hash tables successfully loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 LoadHashTables(const FString& DatasetDirectory, float CellSize, int32 StartTimeStep, int32 EndTimeStep, bool bAutoCreate = true);

	/**
	 * Load a single hash table from disk
	 * 
	 * @param FilePath Full path to the hash table file
	 * @param CellSize Cell size this hash table represents
	 * @param TimeStep Time step this hash table represents
	 * @return True if loaded successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	bool LoadHashTable(const FString& FilePath, float CellSize, int32 TimeStep);

	/**
	 * Create hash tables from trajectory data
	 * 
	 * @param DatasetDirectory Output directory for hash tables
	 * @param CellSize Cell size for the hash tables
	 * @param BoundingBoxMin Minimum bounding box coordinates (if ComputeBoundingBox is false)
	 * @param BoundingBoxMax Maximum bounding box coordinates (if ComputeBoundingBox is false)
	 * @param ComputeBoundingBox Whether to automatically compute bounding box from data
	 * @param BoundingBoxMargin Margin to add to computed bounding box
	 * @return True if hash tables were created successfully
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	bool CreateHashTables(
		const FString& DatasetDirectory,
		float CellSize,
		FVector BoundingBoxMin,
		FVector BoundingBoxMax,
		bool ComputeBoundingBox,
		float BoundingBoxMargin);

	/**
	 * Create hash tables from trajectory data asynchronously (non-blocking)
	 * This method returns immediately and performs the work on background threads.
	 * Progress can be monitored through the completion delegate or by checking IsCreatingHashTables().
	 * After successful creation, the newly created hash tables are automatically loaded on the game thread.
	 * 
	 * Note: Loading happens on the game thread and may cause a brief frame hitch if loading many hash tables.
	 * LoadFromFile() is optimized (only loads headers/entries, not trajectory IDs), but for datasets with
	 * hundreds of timesteps, consider loading in smaller batches separately if frame time is critical.
	 * 
	 * @param DatasetDirectory Output directory for hash tables
	 * @param CellSize Cell size for the hash tables
	 * @param StartTimeStep First time step to create
	 * @param EndTimeStep Last time step to create
	 * @param OnComplete Delegate called when creation completes (success or failure)
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void CreateHashTablesAsync(
		const FString& DatasetDirectory,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep);

	/**
	 * Check if hash table creation is currently in progress
	 * 
	 * @return True if hash tables are being created
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	bool IsCreatingHashTables() const { return bIsCreatingHashTables; }

	/**
	 * Query trajectories within a fixed radius from a position at a specific time step
	 * 
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OutResults Array of trajectory IDs and distances within radius
	 * @return Number of trajectories found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryFixedRadiusNeighbors(
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 TimeStep,
		TArray<FSpatialQueryResult>& OutResults);

	/**
	 * Query all trajectories in the same cell as the query position
	 * 
	 * @param QueryPosition World position to query
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OutTrajectoryIds Array of trajectory IDs in the cell
	 * @return Number of trajectories found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryCell(
		FVector QueryPosition,
		float CellSize,
		int32 TimeStep,
		TArray<int32>& OutTrajectoryIds);

	/**
	 * Query trajectories with actual distance calculation for a single point at a single timestep (Case A)
	 * Returns trajectory samples that are within the query radius after actual distance calculation.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OutResults Array of trajectory query results with sample points
	 * @return Number of trajectories found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryRadiusWithDistanceCheck(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 TimeStep,
		TArray<FSpatialHashQueryResult>& OutResults);

	/**
	 * Query trajectories with dual radius (inner and outer) for a single point at a single timestep
	 * Returns two separate arrays: one for inner radius, one for outer radius only (excluding inner).
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param InnerRadius Inner search radius in world units
	 * @param OuterRadius Outer search radius in world units (must be >= InnerRadius)
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OutInnerResults Trajectories within inner radius
	 * @param OutOuterOnlyResults Trajectories between inner and outer radius
	 * @return Total number of trajectories found (inner + outer)
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryDualRadiusWithDistanceCheck(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float InnerRadius,
		float OuterRadius,
		float CellSize,
		int32 TimeStep,
		TArray<FSpatialHashQueryResult>& OutInnerResults,
		TArray<FSpatialHashQueryResult>& OutOuterOnlyResults);

	/**
	 * Query trajectories over a time range for a single point (Case B)
	 * Returns trajectories that have at least one sample within the query radius during the time range.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query (inclusive)
	 * @param EndTimeStep Last time step to query (inclusive)
	 * @param OutResults Array of trajectory query results with all sample points in time range
	 * @return Number of trajectories found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryRadiusOverTimeRange(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep,
		TArray<FSpatialHashQueryResult>& OutResults);

	/**
	 * Query trajectories for a query trajectory over a time range (Case C)
	 * Returns trajectories that intersect with the query trajectory's radius during the time range.
	 * For each matching trajectory, includes all sample points from when it first enters until it
	 * last exits the query radius (accounting for re-entry).
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryTrajectoryId ID of the query trajectory
	 * @param Radius Search radius around each query point
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query (inclusive)
	 * @param EndTimeStep Last time step to query (inclusive)
	 * @param OutResults Array of trajectory query results with extended sample points
	 * @return Number of trajectories found
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	int32 QueryTrajectoryRadiusOverTimeRange(
		const FString& DatasetDirectory,
		int32 QueryTrajectoryId,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep,
		TArray<FSpatialHashQueryResult>& OutResults);

	/**
	 * Unload hash tables for a specific cell size
	 * 
	 * @param CellSize Cell size to unload
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void UnloadHashTables(float CellSize);

	/**
	 * Unload all hash tables
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void UnloadAllHashTables();

	/**
	 * Get list of loaded cell sizes
	 * 
	 * @param OutCellSizes Array of cell sizes that have loaded hash tables
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void GetLoadedCellSizes(TArray<float>& OutCellSizes) const;

	/**
	 * Get list of loaded time steps for a specific cell size
	 * 
	 * @param CellSize Cell size to query
	 * @param OutTimeSteps Array of time steps that are loaded for this cell size
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void GetLoadedTimeSteps(float CellSize, TArray<int32>& OutTimeSteps) const;

	/**
	 * Check if a hash table is loaded for a specific cell size and time step
	 * 
	 * @param CellSize Cell size to check
	 * @param TimeStep Time step to check
	 * @return True if the hash table is loaded
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	bool IsHashTableLoaded(float CellSize, int32 TimeStep) const;

	/**
	 * Get memory usage statistics
	 * 
	 * @param OutTotalHashTables Total number of loaded hash tables
	 * @param OutTotalMemoryBytes Approximate total memory usage in bytes
	 */
	UFUNCTION(BlueprintCallable, Category = "Spatial Hash")
	void GetMemoryStats(int32& OutTotalHashTables, int64& OutTotalMemoryBytes) const;

	// ============================================================================
	// ASYNC QUERY METHODS (Non-blocking with callbacks)
	// ============================================================================

	/**
	 * Async version: Query trajectories with actual distance calculation for a single point at a single timestep
	 * Uses TrajectoryDataCppApi for non-blocking data loading. Results delivered via callback.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OnComplete Callback invoked with results when query completes
	 */
	void QueryRadiusWithDistanceCheckAsync(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 TimeStep,
		FOnSpatialHashQueryComplete OnComplete);

	/**
	 * Async version: Query trajectories with dual radius for a single point at a single timestep
	 * Uses TrajectoryDataCppApi for non-blocking data loading. Results delivered via callback.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param InnerRadius Inner search radius in world units
	 * @param OuterRadius Outer search radius in world units (must be >= InnerRadius)
	 * @param CellSize Cell size of hash table to use
	 * @param TimeStep Time step to query
	 * @param OnComplete Callback invoked with inner and outer results when query completes
	 */
	void QueryDualRadiusWithDistanceCheckAsync(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float InnerRadius,
		float OuterRadius,
		float CellSize,
		int32 TimeStep,
		FOnSpatialHashDualQueryComplete OnComplete);

	/**
	 * Async version: Query trajectories over a time range for a single point
	 * Uses TrajectoryDataCppApi for non-blocking data loading. Results delivered via callback.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryPosition World position to query
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query (inclusive)
	 * @param EndTimeStep Last time step to query (inclusive)
	 * @param OnComplete Callback invoked with results when query completes
	 */
	void QueryRadiusOverTimeRangeAsync(
		const FString& DatasetDirectory,
		FVector QueryPosition,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep,
		FOnSpatialHashQueryComplete OnComplete);

	/**
	 * Async version: Query trajectories for a query trajectory over a time range
	 * Uses TrajectoryDataCppApi for non-blocking data loading. Results delivered via callback.
	 * 
	 * @param DatasetDirectory Path to dataset containing trajectory data
	 * @param QueryTrajectoryId ID of the query trajectory
	 * @param Radius Search radius in world units
	 * @param CellSize Cell size of hash table to use
	 * @param StartTimeStep First time step to query (inclusive)
	 * @param EndTimeStep Last time step to query (inclusive)
	 * @param OnComplete Callback invoked with results when query completes
	 */
	void QueryTrajectoryRadiusOverTimeRangeAsync(
		const FString& DatasetDirectory,
		uint32 QueryTrajectoryId,
		float Radius,
		float CellSize,
		int32 StartTimeStep,
		int32 EndTimeStep,
		FOnSpatialHashQueryComplete OnComplete);

protected:
	/** Tolerance for floating-point comparison of cell sizes */
	static constexpr float CellSizeEpsilon = 0.001f;

	/**
	 * Key for hash table map: combines cell size and time step
	 */
	struct FHashTableKey
	{
		float CellSize;
		int32 TimeStep;

		FHashTableKey(float InCellSize, int32 InTimeStep)
			: CellSize(InCellSize)
			, TimeStep(InTimeStep)
		{
		}

		bool operator==(const FHashTableKey& Other) const
		{
			return FMath::IsNearlyEqual(CellSize, Other.CellSize, USpatialHashTableManager::CellSizeEpsilon) && TimeStep == Other.TimeStep;
		}

		friend uint32 GetTypeHash(const FHashTableKey& Key)
		{
			return HashCombine(GetTypeHash(Key.CellSize), GetTypeHash(Key.TimeStep));
		}
	};

	/** Map of loaded hash tables */
	TMap<FHashTableKey, TSharedPtr<FSpatialHashTable>> LoadedHashTables;

	/** Thread-safe flag indicating if hash table creation is in progress */
	FThreadSafeBool bIsCreatingHashTables;

	/** Critical section for protecting creation flag */
	FCriticalSection CreationMutex;

	/**
	 * Get a loaded hash table for a specific cell size and time step
	 * 
	 * @param CellSize Cell size
	 * @param TimeStep Time step
	 * @return Pointer to hash table, or nullptr if not loaded
	 */
	TSharedPtr<FSpatialHashTable> GetHashTable(float CellSize, int32 TimeStep) const;

	/**
	 * Check if hash tables exist on disk for the given range
	 * 
	 * @param DatasetDirectory Base directory containing the dataset
	 * @param CellSize Cell size to check for
	 * @param StartTimeStep First time step to check
	 * @param EndTimeStep Last time step to check
	 * @return True if all hash tables exist
	 */
	bool CheckHashTablesExist(const FString& DatasetDirectory, float CellSize, int32 StartTimeStep, int32 EndTimeStep) const;

	/**
	 * Attempt to create hash tables from trajectory data
	 * 
	 * @param DatasetDirectory Base directory containing the dataset
	 * @param CellSize Cell size for the hash tables
	 * @param StartTimeStep First time step to create
	 * @param EndTimeStep Last time step to create
	 * @return True if hash tables were created successfully
	 */
	bool TryCreateHashTables(const FString& DatasetDirectory, float CellSize, int32 StartTimeStep, int32 EndTimeStep);

	/**
	 * Load trajectory data from dataset directory
	 * This method uses LoadShardFile to load complete shard data and processes ALL trajectories
	 * in the dataset, regardless of the time range parameters (which are kept for API compatibility).
	 * 
	 * The method finds all shard files in the dataset directory, loads them using LoadShardFile,
	 * and processes them in parallel while managing memory constraints.
	 * 
	 * @param DatasetDirectory Base directory containing trajectory data (with shard-*.bin files)
	 * @param StartTimeStep Unused - kept for API compatibility (all time steps are processed)
	 * @param EndTimeStep Unused - kept for API compatibility (all time steps are processed)
	 * @param OutTimeStepSamples Output array of trajectory samples for each time step in the complete dataset
	 * @param OutGlobalMinTimeStep Output parameter receiving the first timestep number in the dataset
	 * @return True if trajectory data was loaded successfully
	 */
	bool LoadTrajectoryDataFromDirectory(
		const FString& DatasetDirectory,
		int32 StartTimeStep,
		int32 EndTimeStep,
		TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& OutTimeStepSamples,
		int32& OutGlobalMinTimeStep);

	/**
	 * Build hash tables from shards with batch processing and per-timestep building
	 * This method processes shards in batches. Each batch:
	 * - Loads shard files (each shard contains multiple timesteps)
	 * - Builds one hash table per timestep in parallel
	 * - Writes hash tables to disk
	 * - Frees all batch data before loading next batch
	 * 
	 * No data accumulation across batches - each timestep's hash table is complete
	 * and independent after building from its batch.
	 * 
	 * @param DatasetDirectory Base directory containing trajectory data
	 * @param BaseConfig Base configuration for hash table building
	 * @return True if hash tables were built successfully
	 */
	bool BuildHashTablesIncrementallyFromShards(
		const FString& DatasetDirectory,
		const FSpatialHashTableBuilder::FBuildConfig& BaseConfig);

	/**
	 * Find trajectory positions for distance calculations
	 * This is a placeholder - in a real implementation, this would query the TrajectoryData plugin
	 * 
	 * @param TrajectoryId Trajectory ID
	 * @param TimeStep Time step
	 * @return Position of trajectory at time step
	 */
	FVector GetTrajectoryPosition(int32 TrajectoryId, int32 TimeStep) const;

	/**
	 * Load trajectory sample data for specific trajectory IDs and time range
	 * Loads data from shard files using synchronous LoadShardFile calls.
	 * This method is called from synchronous query methods and uses direct shard loading
	 * to avoid blocking the game thread with busy-waiting on async callbacks.
	 * 
	 * @param DatasetDirectory Base directory containing trajectory data
	 * @param TrajectoryIds Array of trajectory IDs to load
	 * @param StartTimeStep First time step to load (inclusive)
	 * @param EndTimeStep Last time step to load (inclusive)
	 * @param OutTrajectoryData Map from trajectory ID to array of sample points
	 * @return True if data was loaded successfully
	 */
	bool LoadTrajectorySamplesForIds(
		const FString& DatasetDirectory,
		const TArray<uint32>& TrajectoryIds,
		int32 StartTimeStep,
		int32 EndTimeStep,
		TMap<uint32, TArray<FTrajectorySamplePoint>>& OutTrajectoryData) const;

	/**
	 * Find which shard file contains a specific time step
	 * 
	 * @param DatasetDirectory Base directory containing trajectory data
	 * @param TimeStep Time step to find
	 * @return Path to shard file, or empty string if not found
	 */
	FString FindShardFileForTimeStep(const FString& DatasetDirectory, int32 TimeStep) const;

	/**
	 * Compute actual distance between a query point and trajectory samples
	 * Filters trajectory samples by distance and returns those within radius.
	 * 
	 * @param QueryPosition Query point position
	 * @param Radius Search radius
	 * @param TrajectoryData Map of trajectory samples
	 * @param OutResults Filtered results with distance calculations
	 */
	void FilterByDistance(
		const FVector& QueryPosition,
		float Radius,
		const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
		TArray<FSpatialHashQueryResult>& OutResults) const;

	/**
	 * Compute actual distance for dual radius query
	 * Filters trajectory samples into inner and outer radius results.
	 * 
	 * @param QueryPosition Query point position
	 * @param InnerRadius Inner search radius
	 * @param OuterRadius Outer search radius
	 * @param TrajectoryData Map of trajectory samples
	 * @param OutInnerResults Results within inner radius
	 * @param OutOuterOnlyResults Results between inner and outer radius
	 */
	void FilterByDualRadius(
		const FVector& QueryPosition,
		float InnerRadius,
		float OuterRadius,
		const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
		TArray<FSpatialHashQueryResult>& OutInnerResults,
		TArray<FSpatialHashQueryResult>& OutOuterOnlyResults) const;

	/**
	 * Extend trajectory samples to include all points from first entry to last exit
	 * Used for Case C to handle trajectory re-entry into query radius.
	 * 
	 * @param TrajectoryData Trajectory samples with distance information
	 * @param Radius Query radius for determining entry/exit
	 * @param OutExtendedResults Results with extended sample ranges
	 */
	void ExtendTrajectorySamples(
		const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
		float Radius,
		TArray<FSpatialHashQueryResult>& OutExtendedResults) const;

	/**
	 * Helper to parse timestep number from shard filename
	 * @param FilePath Path to shard file (e.g., "/path/shard-3046.bin")
	 * @return Timestep number, or 0 if parsing fails
	 */
	static int32 ParseTimestepFromFilename(const FString& FilePath);

	/**
	 * Get list of shard files from dataset directory
	 * Delegates to TrajectoryData plugin's functionality for discovering shard files.
	 * This centralizes the shard file discovery logic that was duplicated across multiple methods.
	 * 
	 * @param DatasetDirectory Base directory containing trajectory data
	 * @param OutShardFiles Output array of full paths to shard files (sorted)
	 * @return True if successful (directory exists and shard files found), false otherwise
	 */
	bool GetShardFiles(const FString& DatasetDirectory, TArray<FString>& OutShardFiles) const;

	/**
	 * Get or load a hash table, returning a raw pointer for use in async callbacks.
	 * This is a convenience wrapper around GetHashTable() that returns a raw pointer
	 * instead of TSharedPtr for easier use in lambda captures.
	 * 
	 * @param DatasetDirectory Base directory containing the dataset (currently unused, for future loading)
	 * @param CellSize Cell size of the hash table
	 * @param TimeStep Time step of the hash table
	 * @return Raw pointer to hash table, or nullptr if not found
	 */
	FSpatialHashTable* GetOrLoadHashTable(const FString& DatasetDirectory, float CellSize, int32 TimeStep) const;
};
