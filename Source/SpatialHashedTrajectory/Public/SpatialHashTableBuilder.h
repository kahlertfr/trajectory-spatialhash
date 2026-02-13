// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialHashTable.h"

/**
 * Builder class for creating spatial hash tables from trajectory data
 * 
 * This class processes trajectory data and creates spatial hash tables
 * for each time step, organizing trajectories into spatial cells.
 */
class SPATIALHASHEDTRAJECTORY_API FSpatialHashTableBuilder
{
public:
	/**
	 * Configuration for building spatial hash tables
	 */
	struct FBuildConfig
	{
		/** Cell size in world units (uniform in all dimensions) */
		float CellSize;

		/** Bounding box minimum (if not provided, will be computed from data) */
		FVector BBoxMin;

		/** Bounding box maximum (if not provided, will be computed from data) */
		FVector BBoxMax;

		/** Whether to compute bounding box from data (if false, uses provided BBoxMin/BBoxMax) */
		bool bComputeBoundingBox;

		/** Margin to add to computed bounding box (in world units) */
		float BoundingBoxMargin;

		/** Output directory for hash table files */
		FString OutputDirectory;

		/** Number of time steps to process */
		uint32 NumTimeSteps;

		FBuildConfig()
			: CellSize(10.0f)
			, BBoxMin(FVector::ZeroVector)
			, BBoxMax(FVector::ZeroVector)
			, bComputeBoundingBox(true)
			, BoundingBoxMargin(1.0f)
			, OutputDirectory(TEXT(""))
			, NumTimeSteps(0)
		{
		}
	};

	/**
	 * Trajectory sample at a specific time step
	 */
	struct FTrajectorySample
	{
		/** Trajectory ID */
		uint32 TrajectoryId;

		/** Position at this time step */
		FVector Position;

		FTrajectorySample()
			: TrajectoryId(0)
			, Position(FVector::ZeroVector)
		{
		}

		FTrajectorySample(uint32 InId, const FVector& InPosition)
			: TrajectoryId(InId)
			, Position(InPosition)
		{
		}
	};

	FSpatialHashTableBuilder() = default;

	/**
	 * Build spatial hash tables from trajectory data
	 * 
	 * @param Config Build configuration
	 * @param TimeStepSamples Array of trajectory samples for each time step
	 *                        TimeStepSamples[timeStep] contains all samples at that time step
	 * @return true if successful, false otherwise
	 */
	bool BuildHashTables(
		const FBuildConfig& Config,
		const TArray<TArray<FTrajectorySample>>& TimeStepSamples);

	/**
	 * Build a single hash table for one time step
	 * 
	 * @param TimeStep Time step index
	 * @param Samples Trajectory samples at this time step
	 * @param Config Build configuration
	 * @param OutHashTable Output hash table
	 * @return true if successful, false otherwise
	 */
	bool BuildHashTableForTimeStep(
		uint32 TimeStep,
		const TArray<FTrajectorySample>& Samples,
		const FBuildConfig& Config,
		FSpatialHashTable& OutHashTable);

	/**
	 * Compute bounding box from trajectory samples
	 * 
	 * @param TimeStepSamples All trajectory samples across all time steps
	 * @param Margin Margin to add to bounding box (in world units)
	 * @param OutBBoxMin Output bounding box minimum
	 * @param OutBBoxMax Output bounding box maximum
	 */
	static void ComputeBoundingBox(
		const TArray<TArray<FTrajectorySample>>& TimeStepSamples,
		float Margin,
		FVector& OutBBoxMin,
		FVector& OutBBoxMax);

	/**
	 * Generate output filename for a time step
	 * 
	 * @param OutputDirectory Base output directory
	 * @param CellSize Cell size (for subdirectory name)
	 * @param TimeStep Time step index
	 * @return Full path to output file
	 */
	static FString GetOutputFilename(
		const FString& OutputDirectory,
		float CellSize,
		uint32 TimeStep);

	/**
	 * Create directory structure for spatial hash tables
	 * 
	 * @param OutputDirectory Base output directory
	 * @param CellSize Cell size (for subdirectory name)
	 * @return true if successful, false otherwise
	 */
	static bool CreateDirectoryStructure(
		const FString& OutputDirectory,
		float CellSize);

private:
	/**
	 * Helper structure for building hash tables
	 */
	struct FCellData
	{
		/** Z-Order key for this cell */
		uint64 ZOrderKey;

		/** Trajectory IDs in this cell */
		TArray<uint32> TrajectoryIds;

		FCellData(uint64 InKey)
			: ZOrderKey(InKey)
		{
		}
	};
};
