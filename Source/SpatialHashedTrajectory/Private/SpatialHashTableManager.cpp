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
	
	// Note: StartTimeStep and EndTimeStep parameters are now ignored
	// The new implementation processes ALL trajectory data from all shards in the dataset
	// This ensures hash tables are built from complete data
	
	// Load trajectory data from the dataset directory (processes all shards)
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
	int32 GlobalMinTimeStep = 0;
	if (!LoadTrajectoryDataFromDirectory(DatasetDirectory, StartTimeStep, EndTimeStep, TimeStepSamples, GlobalMinTimeStep))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Failed to load trajectory data"));
		return false;
	}
	
	if (TimeStepSamples.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: No trajectory data found"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::TryCreateHashTables: Loaded %d time steps of trajectory data from complete dataset (starting from timestep %d)"),
		TimeStepSamples.Num(), GlobalMinTimeStep);
	
	// Setup configuration for building
	FSpatialHashTableBuilder::FBuildConfig Config;
	Config.CellSize = CellSize;
	Config.bComputeBoundingBox = true;
	Config.BoundingBoxMargin = 1.0f;
	Config.OutputDirectory = DatasetDirectory;
	Config.NumTimeSteps = TimeStepSamples.Num();
	Config.StartTimeStep = GlobalMinTimeStep; // Use actual timestep numbers from shard data
	
	// Build the hash tables (this uses parallel processing internally)
	FSpatialHashTableBuilder Builder;
	if (!Builder.BuildHashTables(Config, TimeStepSamples))
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
		
		// Load trajectory data - check if manager still exists
		// Note: StartTimeStep and EndTimeStep parameters are now ignored
		// The implementation processes ALL trajectory data from all shards in the dataset
		USpatialHashTableManager* Manager = WeakThis.Get();
		if (!Manager)
		{
			return;
		}
		
		TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
		int32 GlobalMinTimeStep = 0;
		if (!Manager->LoadTrajectoryDataFromDirectory(DatasetDirectory, StartTimeStep, EndTimeStep, TimeStepSamples, GlobalMinTimeStep))
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis]()
			{
				if (USpatialHashTableManager* Mgr = WeakThis.Get())
				{
					UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Failed to load trajectory data"));
					Mgr->bIsCreatingHashTables = false;
				}
			});
			return;
		}
		
		if (TimeStepSamples.Num() == 0)
		{
			AsyncTask(ENamedThreads::GameThread, [WeakThis]()
			{
				if (USpatialHashTableManager* Mgr = WeakThis.Get())
				{
					UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::CreateHashTablesAsync: No trajectory data found"));
					Mgr->bIsCreatingHashTables = false;
				}
			});
			return;
		}
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Loaded %d time steps of trajectory data from complete dataset (starting from timestep %d)"),
			TimeStepSamples.Num(), GlobalMinTimeStep);
		
		// Setup configuration for building
		FSpatialHashTableBuilder::FBuildConfig Config;
		Config.CellSize = CellSize;
		Config.bComputeBoundingBox = true;
		Config.BoundingBoxMargin = 1.0f;
		Config.OutputDirectory = DatasetDirectory;
		Config.NumTimeSteps = TimeStepSamples.Num();
		Config.StartTimeStep = GlobalMinTimeStep; // Use actual timestep numbers from shard data
		
		// Build the hash tables (this will use parallel processing internally for time steps,
		// and we've already parallelized shard processing in LoadTrajectoryDataFromDirectory)
		FSpatialHashTableBuilder Builder;
		bool bSuccess = Builder.BuildHashTables(Config, TimeStepSamples);
		
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
	
	// Determine the global time step range from the dataset
	// We'll process all shards to build complete hash tables
	int32 GlobalMinTimeStep = INT32_MAX;
	int32 GlobalMaxTimeStep = INT32_MIN;
	
	// First pass: determine the complete time range
	TArray<FShardFileData> ShardDataArray;
	ShardDataArray.Reserve(ShardFiles.Num());
	
	for (const FString& ShardFile : ShardFiles)
	{
		FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
		if (!ShardData.bSuccess)
		{
			UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to load shard %s: %s"),
				*ShardFile, *ShardData.ErrorMessage);
			continue;
		}
		
		// Calculate the time step range for this shard
		int32 ShardStartTimeStep = ShardData.Header.GlobalIntervalIndex * ShardData.Header.TimeStepIntervalSize;
		int32 ShardEndTimeStep = ShardStartTimeStep + ShardData.Header.TimeStepIntervalSize - 1;
		
		GlobalMinTimeStep = FMath::Min(GlobalMinTimeStep, ShardStartTimeStep);
		GlobalMaxTimeStep = FMath::Max(GlobalMaxTimeStep, ShardEndTimeStep);
		
		ShardDataArray.Add(MoveTemp(ShardData));
	}
	
	if (ShardDataArray.Num() == 0)
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
	
	// Second pass: extract trajectory samples from all shards
	// Process shards in parallel to improve performance
	FCriticalSection SamplesMutex;
	FThreadSafeCounter TotalSamplesProcessed;
	
	ParallelFor(ShardDataArray.Num(), [&](int32 ShardIdx)
	{
		const FShardFileData& ShardData = ShardDataArray[ShardIdx];
		
		// Calculate the time step range for this shard
		int32 ShardStartTimeStep = ShardData.Header.GlobalIntervalIndex * ShardData.Header.TimeStepIntervalSize;
		
		UE_LOG(LogTemp, Verbose, TEXT("Processing shard %d: GlobalIntervalIndex=%d, TimeStepIntervalSize=%d, ShardStartTimeStep=%d"),
			ShardIdx, ShardData.Header.GlobalIntervalIndex, ShardData.Header.TimeStepIntervalSize, ShardStartTimeStep);
		
		// Process each trajectory entry in this shard
		for (const FShardTrajectoryEntry& Entry : ShardData.Entries)
		{
			// Skip entries with no valid samples
			if (Entry.StartTimeStepInInterval == -1 || Entry.ValidSampleCount == 0)
			{
				continue;
			}
			
			// Process each position in the entry
			for (int32 LocalTimeStep = 0; LocalTimeStep < Entry.Positions.Num(); ++LocalTimeStep)
			{
				const FVector3f& Pos = Entry.Positions[LocalTimeStep];
				
				// Skip invalid positions (NaN check)
				if (FMath::IsNaN(Pos.X))
				{
					continue;
				}
				
				// Calculate global time step
				// ShardStartTimeStep: base timestep for the shard interval
				// Entry.StartTimeStepInInterval: offset within interval where this trajectory starts  
				// LocalTimeStep: index into Entry.Positions array
				int32 GlobalTimeStep = ShardStartTimeStep + Entry.StartTimeStepInInterval + LocalTimeStep;
				
				// Debug: Log first sample of first entry for verification
				static bool bLoggedFirstSample = false;
				if (!bLoggedFirstSample && LocalTimeStep == 0)
				{
					UE_LOG(LogTemp, Warning, TEXT("First sample: ShardStartTimeStep=%d, Entry.StartTimeStepInInterval=%d, LocalTimeStep=%d, GlobalTimeStep=%d"),
						ShardStartTimeStep, Entry.StartTimeStepInInterval, LocalTimeStep, GlobalTimeStep);
					bLoggedFirstSample = true;
				}
				
				int32 ArrayIndex = GlobalTimeStep - GlobalMinTimeStep;
				
				if (ArrayIndex >= 0 && ArrayIndex < OutTimeStepSamples.Num())
				{
					// Convert to FSpatialHashTableBuilder format
					FVector Position(Pos.X, Pos.Y, Pos.Z);
					FSpatialHashTableBuilder::FTrajectorySample Sample(static_cast<uint32>(Entry.TrajectoryId), Position);
					
					// Thread-safe addition to the output array
					FScopeLock Lock(&SamplesMutex);
					OutTimeStepSamples[ArrayIndex].Add(Sample);
					TotalSamplesProcessed.Increment();
				}
			}
		}
	});
	
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
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Loaded %d total samples across %d time steps from %d shards"),
		TotalSamplesProcessed.GetValue(), TotalTimeSteps, ShardDataArray.Num());
	
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
