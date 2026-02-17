// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTableBuilder.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/ThreadSafeBool.h"
#include "Misc/Paths.h"
#include "Async/ParallelFor.h"

bool FSpatialHashTableBuilder::BuildHashTables(
	const FBuildConfig& Config,
	const TArray<TArray<FTrajectorySample>>& TimeStepSamples)
{
	if (TimeStepSamples.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: No time step data provided"));
		return false;
	}

	if (Config.CellSize <= 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: Invalid cell size: %f"), Config.CellSize);
		return false;
	}

	// Compute or use provided bounding box
	FVector BBoxMin = Config.BBoxMin;
	FVector BBoxMax = Config.BBoxMax;

	if (Config.bComputeBoundingBox)
	{
		UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: Computing bounding box from data"));
		ComputeBoundingBox(TimeStepSamples, Config.BoundingBoxMargin, BBoxMin, BBoxMax);
		UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: BBox Min: (%f, %f, %f), Max: (%f, %f, %f)"),
			BBoxMin.X, BBoxMin.Y, BBoxMin.Z, BBoxMax.X, BBoxMax.Y, BBoxMax.Z);
	}

	// Create directory structure
	if (!CreateDirectoryStructure(Config.OutputDirectory, Config.CellSize))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: Failed to create directory structure"));
		return false;
	}

	// Build hash table for each time step
	uint32 NumTimeSteps = FMath::Min((uint32)TimeStepSamples.Num(), Config.NumTimeSteps > 0 ? Config.NumTimeSteps : (uint32)TimeStepSamples.Num());
	
	UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: Building hash tables for %u time steps in parallel"), NumTimeSteps);
	UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: Config.StartTimeStep = %u"), Config.StartTimeStep);
	UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: First file will be timestep_%05d.bin"), Config.StartTimeStep);

	// Use thread-safe bool for error tracking
	FThreadSafeBool bHasError(false);
	FCriticalSection ErrorLogMutex;
	
	// Process time steps in parallel
	ParallelFor(NumTimeSteps, [&](int32 TimeStep)
	{
		// Early exit if we've already encountered an error
		if (bHasError)
		{
			return;
		}
		
		FSpatialHashTable HashTable;
		
		// Create modified config with computed bounding box
		FBuildConfig ModifiedConfig = Config;
		ModifiedConfig.BBoxMin = BBoxMin;
		ModifiedConfig.BBoxMax = BBoxMax;
		ModifiedConfig.bComputeBoundingBox = false;

		// Calculate actual timestep number for this array index
		uint32 ActualTimeStep = Config.StartTimeStep + TimeStep;
		
		// Debug: Log first few timesteps
		if (TimeStep < 3)
		{
			FScopeLock Lock(&ErrorLogMutex);
			UE_LOG(LogTemp, Warning, TEXT("Building hash table: ArrayIndex=%d, Config.StartTimeStep=%u, ActualTimeStep=%u"),
				TimeStep, Config.StartTimeStep, ActualTimeStep);
		}

		if (!BuildHashTableForTimeStep(ActualTimeStep, TimeStepSamples[TimeStep], ModifiedConfig, HashTable))
		{
			FScopeLock Lock(&ErrorLogMutex);
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: Failed to build hash table for time step %u"), ActualTimeStep);
			bHasError = true;
			return;
		}

		// Save hash table to file using actual timestep number
		FString Filename = GetOutputFilename(Config.OutputDirectory, Config.CellSize, ActualTimeStep);
		if (!HashTable.SaveToFile(Filename))
		{
			FScopeLock Lock(&ErrorLogMutex);
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: Failed to save hash table for time step %u"), ActualTimeStep);
			bHasError = true;
			return;
		}

		// MEMORY OPTIMIZATION: Free hash table memory immediately after saving to disk
		// This is crucial for large datasets with millions of trajectories
		HashTable.Entries.Empty();
		HashTable.TrajectoryIds.Empty();

		// Log progress at intervals (thread-safe logging)
		if ((TimeStep + 1) % 100 == 0 || TimeStep == (int32)NumTimeSteps - 1)
		{
			FScopeLock Lock(&ErrorLogMutex);
			UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: Processed %u / %u time steps"), TimeStep + 1, NumTimeSteps);
		}
	});

	// Check if any errors occurred
	if (bHasError)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::BuildHashTables: One or more time steps failed to build"));
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::BuildHashTables: Successfully built and saved all hash tables"));
	return true;
}

bool FSpatialHashTableBuilder::BuildHashTableForTimeStep(
	uint32 TimeStep,
	const TArray<FTrajectorySample>& Samples,
	const FBuildConfig& Config,
	FSpatialHashTable& OutHashTable)
{
	// Initialize header
	OutHashTable.Header.TimeStep = TimeStep;
	OutHashTable.Header.CellSize = Config.CellSize;
	OutHashTable.Header.SetBBoxMin(Config.BBoxMin);
	OutHashTable.Header.SetBBoxMax(Config.BBoxMax);

	if (Samples.Num() == 0)
	{
		// Empty time step - valid but no data
		OutHashTable.Header.NumEntries = 0;
		OutHashTable.Header.NumTrajectoryIds = 0;
		return true;
	}

	// ============================================================================
	// CORE ALGORITHM: Build spatial hash table using Z-Order curve indexing
	// ============================================================================
	// This algorithm implements spatial hashing with Z-Order (Morton code) keys
	// to enable efficient spatial queries on trajectory data.
	//
	// Key steps:
	// 1. Partition 3D space into uniform grid cells
	// 2. Map each cell to a Z-Order key (Morton code) for spatial locality
	// 3. Collect all trajectory IDs in each cell
	// 4. Sort by Z-Order keys for efficient binary search queries
	// ============================================================================

	// Map from Z-Order key to cell data
	// This accumulates trajectory IDs for each spatial cell
	TMap<uint64, TArray<uint32>> CellMap;

	// STEP 1: Process each trajectory sample and assign to spatial cells
	for (const FTrajectorySample& Sample : Samples)
	{
		// Convert 3D world position to discrete cell coordinates
		int32 CellX, CellY, CellZ;
		FSpatialHashTable::WorldToCellCoordinates(
			Sample.Position,
			Config.BBoxMin,
			Config.CellSize,
			CellX, CellY, CellZ);

		// STEP 2: Calculate Z-Order key (Morton code) for this cell
		// This interleaves the bits of (x,y,z) to create a single 64-bit key
		// that preserves spatial locality - nearby cells have similar keys
		uint64 Key = FSpatialHashTable::CalculateZOrderKey(CellX, CellY, CellZ);

		// STEP 3: Add trajectory ID to the corresponding cell
		// Multiple trajectories can occupy the same cell
		TArray<uint32>& TrajectoryIds = CellMap.FindOrAdd(Key);
		TrajectoryIds.Add(Sample.TrajectoryId);
	}

	// STEP 4: Convert cell map to hash table entries
	OutHashTable.Entries.Reserve(CellMap.Num());
	OutHashTable.TrajectoryIds.Reserve(Samples.Num());

	// Sort Z-Order keys for consistent ordering and efficient binary search
	TArray<uint64> Keys;
	CellMap.GetKeys(Keys);
	Keys.Sort();

	// STEP 5: Build final hash table structure
	// - Entries array: sorted by Z-Order key, each entry points to trajectory IDs
	// - TrajectoryIds array: flat array of all trajectory IDs, grouped by cell
	uint32 CurrentIndex = 0;
	for (uint64 Key : Keys)
	{
		const TArray<uint32>& TrajectoryIds = CellMap[Key];
		
		// Create hash entry with Z-Order key and index into trajectory IDs array
		FSpatialHashEntry Entry(Key, CurrentIndex, TrajectoryIds.Num());
		OutHashTable.Entries.Add(Entry);

		// Add trajectory IDs to flat array
		for (uint32 TrajectoryId : TrajectoryIds)
		{
			OutHashTable.TrajectoryIds.Add(TrajectoryId);
		}

		CurrentIndex += TrajectoryIds.Num();
	}

	// Update header counts
	OutHashTable.Header.NumEntries = OutHashTable.Entries.Num();
	OutHashTable.Header.NumTrajectoryIds = OutHashTable.TrajectoryIds.Num();

	return true;
}

void FSpatialHashTableBuilder::ComputeBoundingBox(
	const TArray<TArray<FTrajectorySample>>& TimeStepSamples,
	float Margin,
	FVector& OutBBoxMin,
	FVector& OutBBoxMax)
{
	if (TimeStepSamples.Num() == 0)
	{
		OutBBoxMin = FVector::ZeroVector;
		OutBBoxMax = FVector::ZeroVector;
		return;
	}

	// Initialize with extreme values
	OutBBoxMin = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
	OutBBoxMax = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	// Process all samples across all time steps
	for (const TArray<FTrajectorySample>& Samples : TimeStepSamples)
	{
		for (const FTrajectorySample& Sample : Samples)
		{
			OutBBoxMin.X = FMath::Min(OutBBoxMin.X, Sample.Position.X);
			OutBBoxMin.Y = FMath::Min(OutBBoxMin.Y, Sample.Position.Y);
			OutBBoxMin.Z = FMath::Min(OutBBoxMin.Z, Sample.Position.Z);

			OutBBoxMax.X = FMath::Max(OutBBoxMax.X, Sample.Position.X);
			OutBBoxMax.Y = FMath::Max(OutBBoxMax.Y, Sample.Position.Y);
			OutBBoxMax.Z = FMath::Max(OutBBoxMax.Z, Sample.Position.Z);
		}
	}

	// Add margin to bounding box to ensure all points are inside
	OutBBoxMin -= FVector(Margin, Margin, Margin);
	OutBBoxMax += FVector(Margin, Margin, Margin);
}

FString FSpatialHashTableBuilder::GetOutputFilename(
	const FString& OutputDirectory,
	float CellSize,
	uint32 TimeStep)
{
	// Format: <OutputDirectory>/spatial_hashing/cellsize_<X.XXX>/timestep_XXXXX.bin
	FString CellSizeStr = FString::Printf(TEXT("cellsize_%.3f"), CellSize);
	FString TimeStepStr = FString::Printf(TEXT("timestep_%05u.bin"), TimeStep);
	
	return FPaths::Combine(OutputDirectory, TEXT("spatial_hashing"), CellSizeStr, TimeStepStr);
}

bool FSpatialHashTableBuilder::CreateDirectoryStructure(
	const FString& OutputDirectory,
	float CellSize)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Build full directory path
	FString CellSizeStr = FString::Printf(TEXT("cellsize_%.3f"), CellSize);
	FString FullPath = FPaths::Combine(OutputDirectory, TEXT("spatial_hashing"), CellSizeStr);

	// Create directory tree
	if (!PlatformFile.CreateDirectoryTree(*FullPath))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTableBuilder::CreateDirectoryStructure: Failed to create directory: %s"), *FullPath);
		return false;
	}

	UE_LOG(LogTemp, Log, TEXT("FSpatialHashTableBuilder::CreateDirectoryStructure: Created directory: %s"), *FullPath);
	return true;
}
