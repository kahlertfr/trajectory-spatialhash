// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTableManager.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

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
	// Note: This is a placeholder implementation
	// In a real implementation, this would:
	// 1. Check if trajectory data files exist in DatasetDirectory
	// 2. Load trajectory data from the TrajectoryData plugin
	// 3. Convert trajectory data to FSpatialHashTableBuilder::FTrajectorySample format
	// 4. Call FSpatialHashTableBuilder::BuildHashTables() to create the hash tables
	
	UE_LOG(LogTemp, Warning, TEXT("USpatialHashTableManager::TryCreateHashTables: Automatic hash table creation requires integration with TrajectoryData plugin."));
	UE_LOG(LogTemp, Warning, TEXT("To create hash tables for cell size %.3f (time steps %d-%d):"), 
		CellSize, StartTimeStep, EndTimeStep);
	UE_LOG(LogTemp, Warning, TEXT("1. Load trajectory data from the TrajectoryData plugin"));
	UE_LOG(LogTemp, Warning, TEXT("2. Use FSpatialHashTableBuilder to build hash tables with cell size %.3f"), CellSize);
	UE_LOG(LogTemp, Warning, TEXT("3. Or use the CreateHashTables Blueprint function if you have trajectory data available"));
	
	// For now, we'll check if the trajectory data directory structure exists
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	if (!PlatformFile.DirectoryExists(*DatasetDirectory))
	{
		UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Dataset directory does not exist: %s"), 
			*DatasetDirectory);
		return false;
	}
	
	// Setup configuration for building
	FSpatialHashTableBuilder::FBuildConfig Config;
	Config.CellSize = CellSize;
	Config.bComputeBoundingBox = true;
	Config.BoundingBoxMargin = 1.0f;
	Config.OutputDirectory = DatasetDirectory;
	Config.NumTimeSteps = (EndTimeStep - StartTimeStep + 1);
	
	// TODO: This is where you would:
	// - Load trajectory data from files in DatasetDirectory
	// - Convert to TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> format
	// - Call FSpatialHashTableBuilder builder.BuildHashTables(Config, TimeStepSamples)
	
	UE_LOG(LogTemp, Error, TEXT("USpatialHashTableManager::TryCreateHashTables: Trajectory data loading not implemented yet."));
	UE_LOG(LogTemp, Error, TEXT("Please create hash tables manually using the TrajectoryData plugin and FSpatialHashTableBuilder."));
	
	return false;
}

FVector USpatialHashTableManager::GetTrajectoryPosition(int32 TrajectoryId, int32 TimeStep) const
{
	// Placeholder implementation
	// In a real implementation, this would query the TrajectoryData plugin
	// to get the actual position of the trajectory at the given time step

	// Note: Warning is shown in QueryFixedRadiusNeighbors to avoid spam

	return FVector::ZeroVector;
}
