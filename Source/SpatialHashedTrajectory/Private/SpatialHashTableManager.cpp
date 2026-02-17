// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTableManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "TrajectoryDataLoader.h"

USpatialHashTableManager::USpatialHashTableManager()
{
}

int32 USpatialHashTableManager::LoadHashTables(
	const FString& DatasetDirectory,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep,
	bool bAutoCreate)
{
	if (StartTimeStep > EndTimeStep)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadHashTables: StartTimeStep (%d) must be <= EndTimeStep (%d)"),
			StartTimeStep, EndTimeStep);
		return 0;
	}

	// Check if hash tables exist
	bool bHashTablesExist = CheckHashTablesExist(DatasetDirectory, CellSize, StartTimeStep, EndTimeStep);
	
	// If they don't exist and auto-create is enabled, try to create them
	if (!bHashTablesExist && bAutoCreate)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadHashTables: Hash tables not found for cell size %.3f. Attempting to create them..."),
			CellSize);
		
		if (!TryCreateHashTables(DatasetDirectory, CellSize, StartTimeStep, EndTimeStep))
		{
			UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadHashTables: Failed to create hash tables"));
			return 0;
		}
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadHashTables: Successfully created hash tables for cell size %.3f"),
			CellSize);
	}
	else if (!bHashTablesExist)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadHashTables: Hash tables not found and auto-create is disabled"));
		return 0;
	}

	// Now load the hash tables
	int32 LoadedCount = 0;

	for (int32 TimeStep = StartTimeStep; TimeStep <= EndTimeStep; ++TimeStep)
	{
		FString FilePath = FSpatialHashTableBuilder::GetOutputFilename(DatasetDirectory, CellSize, TimeStep);
		
		if (LoadHashTable(FilePath, CellSize, TimeStep))
		{
			LoadedCount++;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadHashTables: Loaded %d/%d hash tables for cell size %.3f"),
		LoadedCount, EndTimeStep - StartTimeStep + 1, CellSize);

	return LoadedCount;
}

bool USpatialHashTableManager::LoadHashTable(const FString& FilePath, float CellSize, int32 TimeStep)
{
	FHashTableKey Key(CellSize, TimeStep);

	// Check if already loaded
	if (LoadedHashTables.Contains(Key))
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadHashTable: Hash table already loaded for cell size %.3f, time step %d"),
			CellSize, TimeStep);
		return true;
	}

	// Create new hash table
	TSharedPtr<FSpatialHashTable> HashTable = MakeShared<FSpatialHashTable>();

	// Load from file
	if (!HashTable->LoadFromFile(FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadHashTable: Failed to load hash table from %s"),
			*FilePath);
		return false;
	}

	// Validate cell size matches
	if (!FMath::IsNearlyEqual(HashTable->Header.CellSize, CellSize, CellSizeEpsilon))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadHashTable: Cell size mismatch. Expected %.3f, got %.3f"),
			CellSize, HashTable->Header.CellSize);
		return false;
	}

	// Validate time step matches
	if (HashTable->Header.TimeStep != (uint32)TimeStep)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadHashTable: Time step mismatch. Expected %d, got %u"),
			TimeStep, HashTable->Header.TimeStep);
		return false;
	}

	// Store in map
	LoadedHashTables.Add(Key, HashTable);

	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadHashTable: Successfully loaded hash table from %s"),
		*FilePath);

	return true;
}

bool USpatialHashTableManager::CreateHashTables(
	const FString& DatasetDirectory,
	float CellSize,
	FVector BoundingBoxMin,
	FVector BoundingBoxMax,
	bool ComputeBoundingBox,
	float BoundingBoxMargin)
{
	// Note: In a real implementation, this would need to get trajectory data from the TrajectoryData plugin
	// For now, this is a placeholder that shows the API structure

	UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::CreateHashTables: This is a placeholder implementation."));
	UE_LOG(LogTemp, Warning, TEXT("To create hash tables, you need to provide trajectory data from the TrajectoryData plugin."));
	UE_LOG(LogTemp, Warning, TEXT("See SpatialHashTableBuilder for the C++ API to build hash tables with actual data."));

	// Setup configuration
	FSpatialHashTableBuilder::FBuildConfig Config;
	Config.CellSize = CellSize;
	Config.BBoxMin = BoundingBoxMin;
	Config.BBoxMax = BoundingBoxMax;
	Config.bComputeBoundingBox = ComputeBoundingBox;
	Config.BoundingBoxMargin = BoundingBoxMargin;
	Config.OutputDirectory = DatasetDirectory;

	// In a real implementation, you would:
	// 1. Get trajectory data from TrajectoryData plugin
	// 2. Convert to FSpatialHashTableBuilder::FTrajectorySample format
	// 3. Call builder.BuildHashTables(Config, TimeStepSamples)

	return false;
}

int32 USpatialHashTableManager::QueryFixedRadiusNeighbors(
	FVector QueryPosition,
	float Radius,
	float CellSize,
	int32 TimeStep,
	TArray<FSpatialQueryResult>& OutResults)
{
	OutResults.Reset();

	// Warning: This method requires GetTrajectoryPosition to be implemented
	// Currently it's a placeholder that returns ZeroVector, so distance calculations will be incorrect
	static bool bWarningShown = false;
	if (!bWarningShown)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::QueryFixedRadiusNeighbors: "
			"GetTrajectoryPosition is a placeholder. Distance calculations will be incorrect until "
			"you integrate with the TrajectoryData plugin to get actual trajectory positions."));
		bWarningShown = true;
	}

	TSharedPtr<FSpatialHashTable> HashTable = GetHashTable(CellSize, TimeStep);
	if (!HashTable.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::QueryFixedRadiusNeighbors: No hash table loaded for cell size %.3f, time step %d"),
			CellSize, TimeStep);
		return 0;
	}

	// Calculate which cells to check based on radius
	float RadiusSq = Radius * Radius;
	int32 CellRadius = FMath::CeilToInt(Radius / CellSize);

	// Get center cell coordinates
	int32 CenterX, CenterY, CenterZ;
	FSpatialHashTable::WorldToCellCoordinates(
		QueryPosition,
		HashTable->Header.GetBBoxMin(),
		HashTable->Header.CellSize,
		CenterX, CenterY, CenterZ);

	// Query neighboring cells
	TSet<uint32> FoundTrajectories;

	for (int32 dx = -CellRadius; dx <= CellRadius; ++dx)
	{
		for (int32 dy = -CellRadius; dy <= CellRadius; ++dy)
		{
			for (int32 dz = -CellRadius; dz <= CellRadius; ++dz)
			{
				int32 CellX = CenterX + dx;
				int32 CellY = CenterY + dy;
				int32 CellZ = CenterZ + dz;

				// Calculate Z-Order key for this cell
				uint64 Key = FSpatialHashTable::CalculateZOrderKey(CellX, CellY, CellZ);

				// Find entry
				int32 EntryIndex = HashTable->FindEntry(Key);
				if (EntryIndex >= 0)
				{
					// Get trajectory IDs for this cell
					TArray<uint32> TrajectoryIds;
					HashTable->GetTrajectoryIdsForCell(EntryIndex, TrajectoryIds);

					// Check distance for each trajectory
					for (uint32 TrajectoryId : TrajectoryIds)
					{
						// Avoid duplicates if trajectory appears in multiple cells
						if (FoundTrajectories.Contains(TrajectoryId))
						{
							continue;
						}

						// Get trajectory position at this time step
						FVector TrajectoryPos = GetTrajectoryPosition(TrajectoryId, TimeStep);

						// Calculate distance
						float DistanceSq = FVector::DistSquared(QueryPosition, TrajectoryPos);

						if (DistanceSq <= RadiusSq)
						{
							FoundTrajectories.Add(TrajectoryId);
							OutResults.Add(FSpatialQueryResult(TrajectoryId, FMath::Sqrt(DistanceSq)));
						}
					}
				}
			}
		}
	}

	// Sort by distance
	OutResults.Sort([](const FSpatialQueryResult& A, const FSpatialQueryResult& B)
	{
		return A.Distance < B.Distance;
	});

	return OutResults.Num();
}

int32 USpatialHashTableManager::QueryCell(
	FVector QueryPosition,
	float CellSize,
	int32 TimeStep,
	TArray<int32>& OutTrajectoryIds)
{
	OutTrajectoryIds.Reset();

	TSharedPtr<FSpatialHashTable> HashTable = GetHashTable(CellSize, TimeStep);
	if (!HashTable.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::QueryCell: No hash table loaded for cell size %.3f, time step %d"),
			CellSize, TimeStep);
		return 0;
	}

	// Query at position
	TArray<uint32> TrajectoryIds;
	if (HashTable->QueryAtPosition(QueryPosition, TrajectoryIds))
	{
		// Convert to int32
		for (uint32 Id : TrajectoryIds)
		{
			OutTrajectoryIds.Add(static_cast<int32>(Id));
		}
	}

	return OutTrajectoryIds.Num();
}

void USpatialHashTableManager::UnloadHashTables(float CellSize)
{
	TArray<FHashTableKey> KeysToRemove;

	for (const auto& Pair : LoadedHashTables)
	{
		if (FMath::IsNearlyEqual(Pair.Key.CellSize, CellSize, CellSizeEpsilon))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (const FHashTableKey& Key : KeysToRemove)
	{
		LoadedHashTables.Remove(Key);
	}

	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::UnloadHashTables: Unloaded %d hash tables for cell size %.3f"),
		KeysToRemove.Num(), CellSize);
}

void USpatialHashTableManager::UnloadAllHashTables()
{
	int32 Count = LoadedHashTables.Num();
	LoadedHashTables.Reset();

	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::UnloadAllHashTables: Unloaded %d hash tables"), Count);
}

void USpatialHashTableManager::GetLoadedCellSizes(TArray<float>& OutCellSizes) const
{
	TSet<float> CellSizes;

	for (const auto& Pair : LoadedHashTables)
	{
		CellSizes.Add(Pair.Key.CellSize);
	}

	OutCellSizes = CellSizes.Array();
	OutCellSizes.Sort();
}

void USpatialHashTableManager::GetLoadedTimeSteps(float CellSize, TArray<int32>& OutTimeSteps) const
{
	OutTimeSteps.Reset();

	for (const auto& Pair : LoadedHashTables)
	{
		if (FMath::IsNearlyEqual(Pair.Key.CellSize, CellSize, CellSizeEpsilon))
		{
			OutTimeSteps.Add(Pair.Key.TimeStep);
		}
	}

	OutTimeSteps.Sort();
}

bool USpatialHashTableManager::IsHashTableLoaded(float CellSize, int32 TimeStep) const
{
	FHashTableKey Key(CellSize, TimeStep);
	return LoadedHashTables.Contains(Key);
}

void USpatialHashTableManager::GetMemoryStats(int32& OutTotalHashTables, int64& OutTotalMemoryBytes) const
{
	OutTotalHashTables = LoadedHashTables.Num();
	OutTotalMemoryBytes = 0;

	for (const auto& Pair : LoadedHashTables)
	{
		const TSharedPtr<FSpatialHashTable>& HashTable = Pair.Value;
		if (HashTable.IsValid())
		{
			// Approximate memory usage
			int64 HeaderSize = sizeof(FSpatialHashHeader);
			int64 EntriesSize = HashTable->Entries.Num() * sizeof(FSpatialHashEntry);
			int64 IdsSize = HashTable->TrajectoryIds.Num() * sizeof(uint32);

			OutTotalMemoryBytes += HeaderSize + EntriesSize + IdsSize;
		}
	}
}

TSharedPtr<FSpatialHashTable> USpatialHashTableManager::GetHashTable(
	float CellSize,
	int32 TimeStep) const
{
	FHashTableKey Key(CellSize, TimeStep);

	// Return the hash table if loaded, otherwise nullptr
	if (LoadedHashTables.Contains(Key))
	{
		return LoadedHashTables[Key];
	}

	return nullptr;
}

bool USpatialHashTableManager::CheckHashTablesExist(
	const FString& DatasetDirectory,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Check if ALL hash tables exist for this cell size
	int32 TotalExpected = EndTimeStep - StartTimeStep + 1;
	int32 ExistingCount = 0;
	
	for (int32 TimeStep = StartTimeStep; TimeStep <= EndTimeStep; ++TimeStep)
	{
		FString FilePath = FSpatialHashTableBuilder::GetOutputFilename(DatasetDirectory, CellSize, TimeStep);
		if (PlatformFile.FileExists(*FilePath))
		{
			ExistingCount++;
		}
	}
	
	// Return true only if ALL requested hash tables exist
	return ExistingCount == TotalExpected;
}

bool USpatialHashTableManager::TryCreateHashTables(
	const FString& DatasetDirectory,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*DatasetDirectory))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Dataset directory does not exist: %s"), 
			*DatasetDirectory);
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::TryCreateHashTables: Creating hash tables for cell size %.3f from complete dataset (processing all shards)"),
		CellSize);
	
	// MEMORY OPTIMIZATION: Build hash tables incrementally per batch
	// Instead of loading all samples then building, we build after each batch
	// This dramatically reduces peak memory usage
	
	// Setup configuration for building
	FSpatialHashTableBuilder::FBuildConfig Config;
	Config.CellSize = CellSize;
	Config.bComputeBoundingBox = true;
	Config.BoundingBoxMargin = 1.0f;
	Config.OutputDirectory = DatasetDirectory;
	
	// Build hash tables incrementally during shard batch processing
	if (!BuildHashTablesIncrementallyFromShards(DatasetDirectory, Config))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Failed to build hash tables"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::TryCreateHashTables: Successfully created hash tables for cell size %.3f"),
		CellSize);
	
	return true;
}

void USpatialHashTableManager::CreateHashTablesAsync(
	const FString& DatasetDirectory,
	float CellSize,
	int32 StartTimeStep,
	int32 EndTimeStep)
{
	// Use critical section to prevent race condition
	FScopeLock Lock(&CreationMutex);
	
	if (bIsCreatingHashTables)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Hash table creation already in progress"));
		return;
	}

	bIsCreatingHashTables = true;

	// Use weak pointer to avoid use-after-free
	TWeakObjectPtr<USpatialHashTableManager> WeakThis(this);

	// Capture parameters by value for the async task
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakThis, DatasetDirectory, CellSize, StartTimeStep, EndTimeStep]()
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
		if (!PlatformFile.DirectoryExists(*DatasetDirectory))
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis, DatasetDirectory]()
			{
				if (USpatialHashTableManager* Manager = WeakThis.Get())
				{
					UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Dataset directory does not exist: %s"), 
						*DatasetDirectory);
					Manager->bIsCreatingHashTables = false;
				}
			});
			return;
		}
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Creating hash tables for cell size %.3f from complete dataset (processing all shards)"),
			CellSize);
		
		// MEMORY OPTIMIZATION: Build hash tables incrementally per batch
		USpatialHashTableManager* Manager = WeakThis.Get();
		if (!Manager)
		{
			return;
		}
		
		// Setup configuration for building
		FSpatialHashTableBuilder::FBuildConfig Config;
		Config.CellSize = CellSize;
		Config.bComputeBoundingBox = true;
		Config.BoundingBoxMargin = 1.0f;
		Config.OutputDirectory = DatasetDirectory;
		
		// Build hash tables incrementally during shard batch processing
		bool bSuccess = Manager->BuildHashTablesIncrementallyFromShards(DatasetDirectory, Config);
		
		// Return to game thread for final logging and cleanup
		AsyncTask(ENamedThreads::GameThread, [WeakThis, bSuccess, CellSize]()
		{
			if (USpatialHashTableManager* Mgr = WeakThis.Get())
			{
				if (bSuccess)
				{
					UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Successfully created hash tables for cell size %.3f"),
						CellSize);
				}
				else
				{
					UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Failed to build hash tables"));
				}
				
				Mgr->bIsCreatingHashTables = false;
			}
		});
	});
}

bool USpatialHashTableManager::BuildHashTablesIncrementallyFromShards(
	const FString& DatasetDirectory,
	const FSpatialHashTableBuilder::FBuildConfig& BaseConfig)
{
	// BATCH PROCESSING WITH PER-TIMESTEP HASH TABLE BUILDING
	// This method processes shards in batches. For each batch:
	// 1. Load batch shards
	// 2. Extract samples organized by timestep
	// 3. Build hash table for each timestep in parallel
	// 4. Write hash tables to disk
	// 5. Free all batch data
	// 
	// Key insight: One shard contains multiple timesteps. Each timestep gets its own
	// hash table file. Hash tables are independent and can be built in parallel.
	// No accumulation across batches - each batch is self-contained.
	
	UTrajectoryDataLoader* Loader = UTrajectoryDataLoader::Get();
	if (!Loader)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::BuildHashTablesIncrementallyFromShards: Failed to get TrajectoryDataLoader"));
		return false;
	}
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DatasetDirectory))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::BuildHashTablesIncrementallyFromShards: Dataset directory does not exist: %s"), 
			*DatasetDirectory);
		return false;
	}
	
	// Find all shard files
	TArray<FString> ShardFiles;
	TArray<FString> AllFiles;
	PlatformFile.IterateDirectory(*DatasetDirectory, [&AllFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			AllFiles.Add(FilenameOrDirectory);
		}
		return true;
	});
	
	for (const FString& File : AllFiles)
	{
		FString Filename = FPaths::GetCleanFilename(File);
		if (Filename.StartsWith(TEXT("shard-")) && Filename.EndsWith(TEXT(".bin")))
		{
			ShardFiles.Add(File);
		}
	}
	
	if (ShardFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::BuildHashTablesIncrementallyFromShards: No shard files found in %s"),
			*DatasetDirectory);
		return false;
	}
	
	ShardFiles.Sort();
	
	// Helper to parse timestep from filename
	auto ParseTimestepFromFilename = [](const FString& FilePath) -> int32
	{
		FString Filename = FPaths::GetCleanFilename(FilePath);
		if (Filename.StartsWith(TEXT("shard-")) && Filename.EndsWith(TEXT(".bin")))
		{
			FString NumberStr = Filename.Mid(6, Filename.Len() - 10);
			return FCString::Atoi(*NumberStr);
		}
		return 0;
	};
	
	// PASS 1: Determine time range and bounding box
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::BuildHashTablesIncrementallyFromShards: Pass 1 - Scanning %d shards for time range and bounding box"),
		ShardFiles.Num());
	
	int32 GlobalMinTimeStep = INT32_MAX;
	int32 GlobalMaxTimeStep = INT32_MIN;
	FVector BBoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
	FVector BBoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
	TArray<int32> ShardStartTimeSteps;
	ShardStartTimeSteps.Reserve(ShardFiles.Num());
	
	for (const FString& ShardFile : ShardFiles)
	{
		FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
		if (!ShardData.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildHashTablesIncrementallyFromShards: Failed to load shard %s: %s"),
				*ShardFile, *ShardData.ErrorMessage);
			continue;
		}
		
		int32 ShardStartTimeStep = ParseTimestepFromFilename(ShardFile);
		int32 ShardEndTimeStep = ShardStartTimeStep + ShardData.Header.TimeStepIntervalSize - 1;
		
		GlobalMinTimeStep = FMath::Min(GlobalMinTimeStep, ShardStartTimeStep);
		GlobalMaxTimeStep = FMath::Max(GlobalMaxTimeStep, ShardEndTimeStep);
		ShardStartTimeSteps.Add(ShardStartTimeStep);
		
		// Compute bounding box if needed
		if (BaseConfig.bComputeBoundingBox)
		{
			for (const FShardTrajectoryEntry& Entry : ShardData.Entries)
			{
				for (const FVector3f& Pos : Entry.Positions)
				{
					if (!FMath::IsNaN(Pos.X) && !FMath::IsNaN(Pos.Y) && !FMath::IsNaN(Pos.Z))
					{
						BBoxMin.X = FMath::Min(BBoxMin.X, (float)Pos.X);
						BBoxMin.Y = FMath::Min(BBoxMin.Y, (float)Pos.Y);
						BBoxMin.Z = FMath::Min(BBoxMin.Z, (float)Pos.Z);
						BBoxMax.X = FMath::Max(BBoxMax.X, (float)Pos.X);
						BBoxMax.Y = FMath::Max(BBoxMax.Y, (float)Pos.Y);
						BBoxMax.Z = FMath::Max(BBoxMax.Z, (float)Pos.Z);
					}
				}
			}
		}
	}
	
	if (ShardStartTimeSteps.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("BuildHashTablesIncrementallyFromShards: Failed to load any shard files"));
		return false;
	}
	
	// Apply bounding box margin
	if (BaseConfig.bComputeBoundingBox)
	{
		FVector Margin(BaseConfig.BoundingBoxMargin, BaseConfig.BoundingBoxMargin, BaseConfig.BoundingBoxMargin);
		BBoxMin -= Margin;
		BBoxMax += Margin;
		
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Computed BBox: Min(%.2f,%.2f,%.2f) Max(%.2f,%.2f,%.2f)"),
			BBoxMin.X, BBoxMin.Y, BBoxMin.Z, BBoxMax.X, BBoxMax.Y, BBoxMax.Z);
	}
	else
	{
		BBoxMin = BaseConfig.BBoxMin;
		BBoxMax = BaseConfig.BBoxMax;
	}
	
	int32 TotalTimeSteps = GlobalMaxTimeStep - GlobalMinTimeStep + 1;
	
	UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Time range: %d to %d (%d steps)"),
		GlobalMinTimeStep, GlobalMaxTimeStep, TotalTimeSteps);
	
	// Create directory structure
	if (!FSpatialHashTableBuilder::CreateDirectoryStructure(BaseConfig.OutputDirectory, BaseConfig.CellSize))
	{
		UE_LOG(LogTemp, Error, TEXT("BuildHashTablesIncrementallyFromShards: Failed to create directory structure"));
		return false;
	}
	
	// Initialize time step samples - one per timestep in THIS batch only
	// We'll determine the timestep range from the current batch
	int32 BatchMinTimeStep = INT32_MAX;
	int32 BatchMaxTimeStep = INT32_MIN;
	
	// PASS 2: Process shards in batches
	// For each batch: load shards → build hash tables → write → free
	const int32 BatchSize = 3;
	int32 TotalShards = ShardFiles.Num();
	
	UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Pass 2 - Processing %d shards in batches of %d"),
		TotalShards, BatchSize);
	
	for (int32 BatchStart = 0; BatchStart < TotalShards; BatchStart += BatchSize)
	{
		int32 BatchEnd = FMath::Min(BatchStart + BatchSize, TotalShards);
		int32 CurrentBatchSize = BatchEnd - BatchStart;
		
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Processing batch %d-%d (%d shards)"),
			BatchStart, BatchEnd - 1, CurrentBatchSize);
		
		// Determine timestep range for this batch
		BatchMinTimeStep = INT32_MAX;
		BatchMaxTimeStep = INT32_MIN;
		
		for (int32 ShardIdx = BatchStart; ShardIdx < BatchEnd; ++ShardIdx)
		{
			int32 ShardStartTimeStep = ShardStartTimeSteps[ShardIdx];
			// We need to re-load to get TimeStepIntervalSize (or store it in pass 1)
			// For now, calculate from our stored data
			BatchMinTimeStep = FMath::Min(BatchMinTimeStep, ShardStartTimeStep);
			// Assume same interval size for estimation (will be corrected below)
		}
		
		// Load current batch of shards
		TArray<FShardFileData> BatchShardData;
		BatchShardData.Reserve(CurrentBatchSize);
		
		for (int32 ShardIdx = BatchStart; ShardIdx < BatchEnd; ++ShardIdx)
		{
			FShardFileData ShardData = Loader->LoadShardFile(ShardFiles[ShardIdx]);
			if (ShardData.bSuccess)
			{
				int32 ShardStartTimeStep = ShardStartTimeSteps[ShardIdx];
				int32 ShardEndTimeStep = ShardStartTimeStep + ShardData.Header.TimeStepIntervalSize - 1;
				BatchMinTimeStep = FMath::Min(BatchMinTimeStep, ShardStartTimeStep);
				BatchMaxTimeStep = FMath::Max(BatchMaxTimeStep, ShardEndTimeStep);
				BatchShardData.Add(MoveTemp(ShardData));
			}
		}
		
		if (BatchShardData.Num() == 0)
		{
			UE_LOG(LogTemp, Warning, TEXT("BuildHashTablesIncrementallyFromShards: No valid shards in batch %d-%d"),
				BatchStart, BatchEnd - 1);
			continue;
		}
		
		int32 BatchTimeSteps = BatchMaxTimeStep - BatchMinTimeStep + 1;
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Batch timestep range: %d to %d (%d timesteps)"),
			BatchMinTimeStep, BatchMaxTimeStep, BatchTimeSteps);
		
		// Extract samples from batch, organized by timestep
		TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> BatchTimeStepSamples;
		BatchTimeStepSamples.SetNum(BatchTimeSteps);
		
		FCriticalSection SamplesMutex;
		FThreadSafeCounter BatchSamplesProcessed;
		
		// Extract samples from all shards in this batch
		ParallelFor(BatchShardData.Num(), [&](int32 BatchIdx)
		{
			const FShardFileData& ShardData = BatchShardData[BatchIdx];
			int32 ShardStartTimeStep = ShardStartTimeSteps[BatchStart + BatchIdx];
			
			for (const FShardTrajectoryEntry& Entry : ShardData.Entries)
			{
				if (Entry.ValidSampleCount == 0) continue;
				
				for (int32 LocalTimeStep = 0; LocalTimeStep < Entry.Positions.Num(); ++LocalTimeStep)
				{
					const FVector3f& Pos = Entry.Positions[LocalTimeStep];
					if (FMath::IsNaN(Pos.X) || FMath::IsNaN(Pos.Y) || FMath::IsNaN(Pos.Z))
						continue;
					
					int32 GlobalTimeStep = ShardStartTimeStep + LocalTimeStep;
					int32 ArrayIndex = GlobalTimeStep - BatchMinTimeStep;
					
					if (ArrayIndex >= 0 && ArrayIndex < BatchTimeStepSamples.Num())
					{
						FVector Position(Pos.X, Pos.Y, Pos.Z);
						FSpatialHashTableBuilder::FTrajectorySample Sample(static_cast<uint32>(Entry.TrajectoryId), Position);
						
						FScopeLock Lock(&SamplesMutex);
						BatchTimeStepSamples[ArrayIndex].Add(Sample);
						BatchSamplesProcessed.Increment();
					}
				}
			}
		});
		
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Batch %d-%d extracted %d samples"),
			BatchStart, BatchEnd - 1, BatchSamplesProcessed.GetValue());
		
		// CRITICAL: Free batch shard data immediately after extraction
		BatchShardData.Empty();
		
		// Build hash tables for each timestep in this batch (in parallel)
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Building %d hash tables in parallel"),
			BatchTimeSteps);
		
		FThreadSafeBool bBuildError(false);
		FCriticalSection ErrorLogMutex;
		
		ParallelFor(BatchTimeSteps, [&](int32 TimeStepIdx)
		{
			if (bBuildError) return;
			
			int32 GlobalTimeStep = BatchMinTimeStep + TimeStepIdx;
			const TArray<FSpatialHashTableBuilder::FTrajectorySample>& Samples = BatchTimeStepSamples[TimeStepIdx];
			
			// Skip empty timesteps
			if (Samples.Num() == 0)
			{
				return;
			}
			
			// Build hash table for this single timestep
			FSpatialHashTable HashTable;
			
			FSpatialHashTableBuilder::FBuildConfig TimeStepConfig = BaseConfig;
			TimeStepConfig.BBoxMin = BBoxMin;
			TimeStepConfig.BBoxMax = BBoxMax;
			TimeStepConfig.bComputeBoundingBox = false;
			
			FSpatialHashTableBuilder Builder;
			if (!Builder.BuildHashTableForTimeStep(GlobalTimeStep, Samples, TimeStepConfig, HashTable))
			{
				FScopeLock Lock(&ErrorLogMutex);
				UE_LOG(LogTemp, Error, TEXT("BuildHashTablesIncrementallyFromShards: Failed to build hash table for timestep %d"),
					GlobalTimeStep);
				bBuildError = true;
				return;
			}
			
			// Write hash table to disk immediately
			FString Filename = FSpatialHashTableBuilder::GetOutputFilename(BaseConfig.OutputDirectory, BaseConfig.CellSize, GlobalTimeStep);
			if (!HashTable.SaveToFile(Filename))
			{
				FScopeLock Lock(&ErrorLogMutex);
				UE_LOG(LogTemp, Error, TEXT("BuildHashTablesIncrementallyFromShards: Failed to save hash table for timestep %d"),
					GlobalTimeStep);
				bBuildError = true;
				return;
			}
		});
		
		if (bBuildError)
		{
			UE_LOG(LogTemp, Error, TEXT("BuildHashTablesIncrementallyFromShards: Failed to build hash tables for batch %d-%d"),
				BatchStart, BatchEnd - 1);
			return false;
		}
		
		// CRITICAL: Free all batch timestep samples before loading next batch
		BatchTimeStepSamples.Empty();
		
		UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Batch %d-%d complete, %d hash tables built and saved, all data freed"),
			BatchStart, BatchEnd - 1, BatchTimeSteps);
	}
	
	UE_LOG(LogTemp, Log, TEXT("BuildHashTablesIncrementallyFromShards: Successfully completed incremental hash table building"));
	return true;
}

bool USpatialHashTableManager::LoadTrajectoryDataFromDirectory(
	const FString& DatasetDirectory,
	int32 StartTimeStep,
	int32 EndTimeStep,
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& OutTimeStepSamples,
	int32& OutGlobalMinTimeStep)
{
	OutTimeStepSamples.Reset();
	OutGlobalMinTimeStep = 0;
	
	// Use the trajectory data plugin to load shards
	UTrajectoryDataLoader* Loader = UTrajectoryDataLoader::Get();
	if (!Loader)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to get TrajectoryDataLoader. Please ensure the TrajectoryData plugin is properly installed and enabled."));
		return false;
	}
	
	// Find all shard files in the dataset directory
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*DatasetDirectory))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Dataset directory does not exist: %s"), 
			*DatasetDirectory);
		return false;
	}
	
	TArray<FString> ShardFiles;
	// Find all files in the dataset directory using visitor pattern
	TArray<FString> AllFiles;
	PlatformFile.IterateDirectory(*DatasetDirectory, [&AllFiles](const TCHAR* FilenameOrDirectory, bool bIsDirectory) -> bool
	{
		if (!bIsDirectory)
		{
			AllFiles.Add(FilenameOrDirectory);
		}
		return true; // Continue iteration
	});
	
	// Filter to keep only shard-*.bin files
	for (const FString& File : AllFiles)
	{
		FString Filename = FPaths::GetCleanFilename(File);
		if (Filename.StartsWith(TEXT("shard-")) && Filename.EndsWith(TEXT(".bin")))
		{
			ShardFiles.Add(File);
		}
	}
	
	if (ShardFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No shard files found in %s"),
			*DatasetDirectory);
		return false;
	}
	
	// Sort shard files by name to ensure consistent ordering
	ShardFiles.Sort();
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Found %d shard files"), ShardFiles.Num());
	
	// Helper function to parse timestep from shard filename (e.g., "shard-3046.bin" -> 3046)
	auto ParseTimestepFromFilename = [](const FString& FilePath) -> int32
	{
		FString Filename = FPaths::GetCleanFilename(FilePath);
		// Remove "shard-" prefix and ".bin" suffix
		if (Filename.StartsWith(TEXT("shard-")) && Filename.EndsWith(TEXT(".bin")))
		{
			FString NumberStr = Filename.Mid(6, Filename.Len() - 10); // Extract number between "shard-" and ".bin"
			return FCString::Atoi(*NumberStr);
		}
		return 0;
	};
	
	// Determine the global time step range from the dataset
	// We'll process all shards to build complete hash tables
	int32 GlobalMinTimeStep = INT32_MAX;
	int32 GlobalMaxTimeStep = INT32_MIN;
	
	// MEMORY OPTIMIZATION: First pass - lightweight scan to determine time range
	// We need to know the time range before we can initialize OutTimeStepSamples,
	// but we don't want to load all shard data at once.
	// So we'll do a lightweight pass to get just the time range from each shard.
	TArray<int32> ShardStartTimeSteps;
	ShardStartTimeSteps.Reserve(ShardFiles.Num());
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: First pass - determining time range from %d shards"), ShardFiles.Num());
	
	for (const FString& ShardFile : ShardFiles)
	{
		// Load shard to get its time range
		FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
		if (!ShardData.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to load shard %s: %s"),
				*ShardFile, *ShardData.ErrorMessage);
			continue;
		}
		
		// Parse the starting timestep from the shard filename
		int32 ShardStartTimeStep = ParseTimestepFromFilename(ShardFile);
		
		// Calculate the time step range for this shard
		int32 ShardEndTimeStep = ShardStartTimeStep + ShardData.Header.TimeStepIntervalSize - 1;
		
		UE_LOG(LogTemp, Verbose, TEXT("Shard %s: timestep=%d, size=%d, range: %d to %d"),
			*FPaths::GetCleanFilename(ShardFile), ShardStartTimeStep, ShardData.Header.TimeStepIntervalSize,
			ShardStartTimeStep, ShardEndTimeStep);
		
		GlobalMinTimeStep = FMath::Min(GlobalMinTimeStep, ShardStartTimeStep);
		GlobalMaxTimeStep = FMath::Max(GlobalMaxTimeStep, ShardEndTimeStep);
		
		// Store starting timestep for second pass
		ShardStartTimeSteps.Add(ShardStartTimeStep);
		
		// IMPORTANT: Shard data freed automatically when ShardData goes out of scope
		// This prevents loading all shards into memory at once
	}
	
	if (ShardStartTimeSteps.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to load any shard files"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Global time step range: %d to %d"),
		GlobalMinTimeStep, GlobalMaxTimeStep);
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: GlobalMinTimeStep = %d will be used as StartTimeStep"),
		GlobalMinTimeStep);
	
	// Store the global minimum timestep for the caller
	OutGlobalMinTimeStep = GlobalMinTimeStep;
	
	// Initialize output array for all time steps in the dataset
	int32 TotalTimeSteps = GlobalMaxTimeStep - GlobalMinTimeStep + 1;
	OutTimeStepSamples.SetNum(TotalTimeSteps);
	
	// MEMORY OPTIMIZATION: Second pass - process shards in batches
	// Process 3 shards at a time (configurable) to prevent memory overflow
	const int32 BatchSize = 3; // Configurable: 2-3 for memory-constrained systems, 5-10 for high-memory systems
	int32 TotalShards = ShardFiles.Num();
	int32 TotalSamplesProcessed = 0;
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Processing %d shards in batches of %d"),
		TotalShards, BatchSize);
	
	FCriticalSection SamplesMutex;
	
	// Process shards in batches
	for (int32 BatchStart = 0; BatchStart < TotalShards; BatchStart += BatchSize)
	{
		int32 BatchEnd = FMath::Min(BatchStart + BatchSize, TotalShards);
		int32 CurrentBatchSize = BatchEnd - BatchStart;
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Processing batch %d-%d (%d shards)"),
			BatchStart, BatchEnd - 1, CurrentBatchSize);
		
		// Load current batch of shards
		TArray<FShardFileData> BatchShardData;
		TArray<int32> BatchShardStartTimeSteps;
		BatchShardData.Reserve(CurrentBatchSize);
		BatchShardStartTimeSteps.Reserve(CurrentBatchSize);
		
		for (int32 ShardIdx = BatchStart; ShardIdx < BatchEnd; ++ShardIdx)
		{
			const FString& ShardFile = ShardFiles[ShardIdx];
			FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
			
			if (!ShardData.bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to load shard %s in batch: %s"),
					*ShardFile, *ShardData.ErrorMessage);
				continue;
			}
			
			UE_LOG(LogTemp, Verbose, TEXT("Loaded shard %s for processing (batch %d-%d)"),
				*FPaths::GetCleanFilename(ShardFile), BatchStart, BatchEnd - 1);
			
			BatchShardData.Add(MoveTemp(ShardData));
			BatchShardStartTimeSteps.Add(ShardStartTimeSteps[ShardIdx]);
		}
		
		// Process current batch in parallel
		FThreadSafeCounter BatchSamplesProcessed;
		
		ParallelFor(BatchShardData.Num(), [&](int32 BatchIdx)
		{
			const FShardFileData& ShardData = BatchShardData[BatchIdx];
			int32 ShardStartTimeStep = BatchShardStartTimeSteps[BatchIdx];
			
			// Process each trajectory entry in this shard
			for (const FShardTrajectoryEntry& Entry : ShardData.Entries)
			{
				// Skip entries with no valid samples
				if (Entry.ValidSampleCount == 0)
				{
					continue;
				}
				
				// Process each position in the entry
				for (int32 LocalTimeStep = 0; LocalTimeStep < Entry.Positions.Num(); ++LocalTimeStep)
				{
					const FVector3f& Pos = Entry.Positions[LocalTimeStep];
					
					// Skip invalid positions (NaN check for all components)
					if (FMath::IsNaN(Pos.X) || FMath::IsNaN(Pos.Y) || FMath::IsNaN(Pos.Z))
					{
						continue;
					}
					
					// Calculate global time step
					int32 GlobalTimeStep = ShardStartTimeStep + LocalTimeStep;
					int32 ArrayIndex = GlobalTimeStep - GlobalMinTimeStep;
					
					if (ArrayIndex >= 0 && ArrayIndex < OutTimeStepSamples.Num())
					{
						// Convert to FSpatialHashTableBuilder format
						FVector Position(Pos.X, Pos.Y, Pos.Z);
						FSpatialHashTableBuilder::FTrajectorySample Sample(static_cast<uint32>(Entry.TrajectoryId), Position);
						
						// Thread-safe addition to the output array
						FScopeLock Lock(&SamplesMutex);
						OutTimeStepSamples[ArrayIndex].Add(Sample);
						BatchSamplesProcessed.Increment();
					}
				}
			}
		});
		
		TotalSamplesProcessed += BatchSamplesProcessed.GetValue();
		
		// Free batch data immediately before loading next batch
		BatchShardData.Empty();
		BatchShardStartTimeSteps.Empty();
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Completed batch %d-%d, processed %d samples (total: %d)"),
			BatchStart, BatchEnd - 1, BatchSamplesProcessed.GetValue(), TotalSamplesProcessed);
	}
	
	// Free metadata array
	ShardStartTimeSteps.Empty();
	
	// Verify we have at least some data
	bool bHasData = false;
	for (const auto& TimeStepData : OutTimeStepSamples)
	{
		if (TimeStepData.Num() > 0)
		{
			bHasData = true;
			break;
		}
	}
	
	if (!bHasData)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No valid trajectory samples were loaded"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Loaded %d total samples across %d time steps from %d shards using batch processing"),
		TotalSamplesProcessed, TotalTimeSteps, TotalShards);
	
	return true;
}

FVector USpatialHashTableManager::GetTrajectoryPosition(int32 TrajectoryId, int32 TimeStep) const
{
	// Placeholder implementation
	// In a real implementation, this would query the TrajectoryData plugin
	// to get the actual position of the trajectory at the given time step

	// Note: Warning is shown in QueryFixedRadiusNeighbors to avoid spam

	return FVector::ZeroVector;
}
