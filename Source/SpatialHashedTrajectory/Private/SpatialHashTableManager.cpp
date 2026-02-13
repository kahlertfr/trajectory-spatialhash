// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTableManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Async/Async.h"

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
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::TryCreateHashTables: Creating hash tables for cell size %.3f (time steps %d-%d)"),
		CellSize, StartTimeStep, EndTimeStep);
	
	// Load trajectory data from the dataset directory
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
	if (!LoadTrajectoryDataFromDirectory(DatasetDirectory, StartTimeStep, EndTimeStep, TimeStepSamples))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Failed to load trajectory data"));
		return false;
	}
	
	if (TimeStepSamples.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: No trajectory data found"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::TryCreateHashTables: Loaded %d time steps of trajectory data"),
		TimeStepSamples.Num());
	
	// Setup configuration for building
	FSpatialHashTableBuilder::FBuildConfig Config;
	Config.CellSize = CellSize;
	Config.bComputeBoundingBox = true;
	Config.BoundingBoxMargin = 1.0f;
	Config.OutputDirectory = DatasetDirectory;
	Config.NumTimeSteps = TimeStepSamples.Num();
	
	// Build the hash tables
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
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Creating hash tables for cell size %.3f (time steps %d-%d)"),
			CellSize, StartTimeStep, EndTimeStep);
		
		// Load trajectory data - check if manager still exists
		USpatialHashTableManager* Manager = WeakThis.Get();
		if (!Manager)
		{
			return;
		}
		
		TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
		if (!Manager->LoadTrajectoryDataFromDirectory(DatasetDirectory, StartTimeStep, EndTimeStep, TimeStepSamples))
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
		
		UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::CreateHashTablesAsync: Loaded %d time steps of trajectory data"),
			TimeStepSamples.Num());
		
		// Setup configuration for building
		FSpatialHashTableBuilder::FBuildConfig Config;
		Config.CellSize = CellSize;
		Config.bComputeBoundingBox = true;
		Config.BoundingBoxMargin = 1.0f;
		Config.OutputDirectory = DatasetDirectory;
		Config.NumTimeSteps = TimeStepSamples.Num();
		
		// Build the hash tables (this will use parallel processing internally)
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
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& OutTimeStepSamples)
{
	OutTimeStepSamples.Reset();
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Look for trajectory data shard files in the dataset directory
	// Expected file pattern: trajectory_shard_*.bin or similar
	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *DatasetDirectory, TEXT(".bin"));
	
	if (FoundFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No .bin files found in %s"),
			*DatasetDirectory);
		UE_LOG(LogTemp, Warning, TEXT("Looking for trajectory shard files..."));
		
		// Try looking for other common extensions
		PlatformFile.FindFilesRecursively(FoundFiles, *DatasetDirectory, TEXT(".traj"));
		PlatformFile.FindFilesRecursively(FoundFiles, *DatasetDirectory, TEXT(".dat"));
		
		if (FoundFiles.Num() == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No trajectory data files found"));
			return false;
		}
	}
	
	// Filter out spatial hash table files (they contain "spatial" or "hash" in the path)
	TArray<FString> TrajectoryFiles;
	for (const FString& File : FoundFiles)
	{
		FString LowerFile = File.ToLower();
		if (!LowerFile.Contains(TEXT("spatial")) && !LowerFile.Contains(TEXT("hash")))
		{
			TrajectoryFiles.Add(File);
		}
	}
	
	if (TrajectoryFiles.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No trajectory data files found (filtered out spatial hash files)"));
		return false;
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Found %d potential trajectory files"),
		TrajectoryFiles.Num());
	
	// Initialize the output array for the requested time steps
	int32 NumTimeSteps = EndTimeStep - StartTimeStep + 1;
	OutTimeStepSamples.SetNum(NumTimeSteps);
	
	// Try to load each file and extract trajectory samples
	for (const FString& FilePath : TrajectoryFiles)
	{
		if (!LoadTrajectorySamplesFromFile(FilePath, StartTimeStep, EndTimeStep, OutTimeStepSamples))
		{
			UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Failed to load from %s"),
				*FilePath);
		}
	}
	
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
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: No trajectory samples were loaded"));
		return false;
	}
	
	// Log summary
	int32 TotalSamples = 0;
	for (const auto& TimeStepData : OutTimeStepSamples)
	{
		TotalSamples += TimeStepData.Num();
	}
	
	UE_LOG(LogTemp, Log, TEXT("USpatialHashTableManager::LoadTrajectoryDataFromDirectory: Loaded %d total samples across %d time steps"),
		TotalSamples, NumTimeSteps);
	
	return true;
}

bool USpatialHashTableManager::LoadTrajectorySamplesFromFile(
	const FString& FilePath,
	int32 StartTimeStep,
	int32 EndTimeStep,
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& InOutTimeStepSamples)
{
	// Binary file format constants
	static const uint32 TRAJ_MAGIC_1 = 0x5452414A; // "TRAJ"
	static const uint32 TRAJ_MAGIC_2 = 0x54524144; // "TRAD"
	static const int32 HEADER_SIZE = 64;
	static const int32 MIN_FILE_SIZE = 64;
	
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* FileHandle = PlatformFile.OpenRead(*FilePath);
	
	if (!FileHandle)
	{
		return false;
	}
	
	int64 FileSize = FileHandle->Size();
	if (FileSize < MIN_FILE_SIZE)
	{
		delete FileHandle;
		return false;
	}
	
	// Read the entire file into a buffer
	TArray<uint8> Buffer;
	Buffer.SetNum(FileSize);
	
	if (!FileHandle->Read(Buffer.GetData(), FileSize))
	{
		delete FileHandle;
		return false;
	}
	
	delete FileHandle;
	
	// Parse the buffer as trajectory data
	int64 Offset = 0;
	
	// Read header using safe memory copy
	uint32 MagicNumber;
	FMemory::Memcpy(&MagicNumber, &Buffer[Offset], sizeof(uint32));
	Offset += sizeof(uint32);
	
	// Check for trajectory data magic numbers
	if (MagicNumber != TRAJ_MAGIC_1 && MagicNumber != TRAJ_MAGIC_2)
	{
		// Try as simple CSV/text format
		return LoadTrajectorySamplesFromTextFile(FilePath, StartTimeStep, EndTimeStep, InOutTimeStepSamples);
	}
	
	// Read version
	uint32 Version;
	FMemory::Memcpy(&Version, &Buffer[Offset], sizeof(uint32));
	Offset += sizeof(uint32);
	
	// Read number of trajectories
	uint32 NumTrajectories;
	FMemory::Memcpy(&NumTrajectories, &Buffer[Offset], sizeof(uint32));
	Offset += sizeof(uint32);
	
	// Read number of time steps in file
	uint32 NumTimeStepsInFile;
	FMemory::Memcpy(&NumTimeStepsInFile, &Buffer[Offset], sizeof(uint32));
	Offset += sizeof(uint32);
	
	// Skip remaining header
	Offset = HEADER_SIZE;
	
	// Read trajectory data for each time step
	for (uint32 FileTimeStep = 0; FileTimeStep < NumTimeStepsInFile && Offset < FileSize; ++FileTimeStep)
	{
		int32 OutputTimeStep = static_cast<int32>(FileTimeStep);
		
		// Read number of samples for this time step
		if (Offset + sizeof(uint32) > FileSize) break;
		
		uint32 NumSamples;
		FMemory::Memcpy(&NumSamples, &Buffer[Offset], sizeof(uint32));
		Offset += sizeof(uint32);
		
		// Calculate bytes needed for this time step's data
		int64 SamplesDataSize = static_cast<int64>(NumSamples) * (sizeof(uint32) + 3 * sizeof(float));
		
		// Check if this time step is in our requested range
		bool bInRange = (OutputTimeStep >= StartTimeStep && OutputTimeStep <= EndTimeStep);
		
		if (!bInRange)
		{
			// Skip this time step's data
			Offset += SamplesDataSize;
			continue;
		}
		
		// Verify we have enough data for all samples
		if (Offset + SamplesDataSize > FileSize)
		{
			break;
		}
		
		int32 ArrayIndex = OutputTimeStep - StartTimeStep;
		if (ArrayIndex < 0 || ArrayIndex >= InOutTimeStepSamples.Num())
		{
			// Skip data but advance offset
			Offset += SamplesDataSize;
			continue;
		}
		
		// Read samples (trajectory ID + position)
		for (uint32 i = 0; i < NumSamples; ++i)
		{
			uint32 TrajectoryId;
			FMemory::Memcpy(&TrajectoryId, &Buffer[Offset], sizeof(uint32));
			Offset += sizeof(uint32);
			
			float X, Y, Z;
			FMemory::Memcpy(&X, &Buffer[Offset], sizeof(float));
			Offset += sizeof(float);
			FMemory::Memcpy(&Y, &Buffer[Offset], sizeof(float));
			Offset += sizeof(float);
			FMemory::Memcpy(&Z, &Buffer[Offset], sizeof(float));
			Offset += sizeof(float);
			
			FSpatialHashTableBuilder::FTrajectorySample Sample(TrajectoryId, FVector(X, Y, Z));
			InOutTimeStepSamples[ArrayIndex].Add(Sample);
		}
	}
	
	return true;
}

bool USpatialHashTableManager::LoadTrajectorySamplesFromTextFile(
	const FString& FilePath,
	int32 StartTimeStep,
	int32 EndTimeStep,
	TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>>& InOutTimeStepSamples)
{
	// Try to load as CSV/text format
	// Expected format: TimeStep,TrajectoryID,X,Y,Z
	
	FString FileContent;
	if (!FFileHelper::LoadFileToString(FileContent, *FilePath))
	{
		return false;
	}
	
	TArray<FString> Lines;
	FileContent.ParseIntoArrayLines(Lines);
	
	if (Lines.Num() == 0)
	{
		return false;
	}
	
	bool bHasData = false;
	
	// Skip header if present
	int32 StartLine = 0;
	if (Lines[0].Contains(TEXT("TimeStep")) || Lines[0].Contains(TEXT("time")))
	{
		StartLine = 1;
	}
	
	for (int32 i = StartLine; i < Lines.Num(); ++i)
	{
		FString Line = Lines[i].TrimStartAndEnd();
		if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
		{
			continue;
		}
		
		TArray<FString> Fields;
		Line.ParseIntoArray(Fields, TEXT(","));
		
		if (Fields.Num() < 5)
		{
			// Try tab-separated
			Fields.Reset();
			Line.ParseIntoArray(Fields, TEXT("\t"));
		}
		
		if (Fields.Num() >= 5)
		{
			int32 TimeStep = FCString::Atoi(*Fields[0]);
			
			if (TimeStep >= StartTimeStep && TimeStep <= EndTimeStep)
			{
				int32 ArrayIndex = TimeStep - StartTimeStep;
				if (ArrayIndex >= 0 && ArrayIndex < InOutTimeStepSamples.Num())
				{
					uint32 TrajectoryId = FCString::Atoi(*Fields[1]);
					float X = FCString::Atof(*Fields[2]);
					float Y = FCString::Atof(*Fields[3]);
					float Z = FCString::Atof(*Fields[4]);
					
					FSpatialHashTableBuilder::FTrajectorySample Sample(TrajectoryId, FVector(X, Y, Z));
					InOutTimeStepSamples[ArrayIndex].Add(Sample);
					bHasData = true;
				}
			}
		}
	}
	
	return bHasData;
}

FVector USpatialHashTableManager::GetTrajectoryPosition(int32 TrajectoryId, int32 TimeStep) const
{
	// Placeholder implementation
	// In a real implementation, this would query the TrajectoryData plugin
	// to get the actual position of the trajectory at the given time step

	// Note: Warning is shown in QueryFixedRadiusNeighbors to avoid spam

	return FVector::ZeroVector;
}
