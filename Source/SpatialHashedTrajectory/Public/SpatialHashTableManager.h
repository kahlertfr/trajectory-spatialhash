// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SpatialHashTable.h"
#include "SpatialHashTableBuilder.h"
#include "SpatialHashTableManager.generated.h"

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
	 * Find trajectory positions for distance calculations
	 * This is a placeholder - in a real implementation, this would query the TrajectoryData plugin
	 * 
	 * @param TrajectoryId Trajectory ID
	 * @param TimeStep Time step
	 * @return Position of trajectory at time step
	 */
	FVector GetTrajectoryPosition(int32 TrajectoryId, int32 TimeStep) const;
};
