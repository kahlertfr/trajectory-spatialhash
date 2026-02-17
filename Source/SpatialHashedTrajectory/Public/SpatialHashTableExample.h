// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SpatialHashTable.h"
#include "SpatialHashTableBuilder.h"

/**
 * Example usage and validation functions for spatial hash tables
 * 
 * This file demonstrates how to use the spatial hash table API
 * and provides validation functions to test the implementation.
 */
namespace SpatialHashTableExample
{
	/**
	 * Create a simple example hash table with test data
	 * 
	 * @param OutHashTable Output hash table populated with test data
	 */
	inline void CreateExampleHashTable(FSpatialHashTable& OutHashTable)
	{
		// Setup header
		OutHashTable.Header.TimeStep = 0;
		OutHashTable.Header.CellSize = 10.0f;
		OutHashTable.Header.BBoxMin = FVector(0.0f, 0.0f, 0.0f);
		OutHashTable.Header.BBoxMax = FVector(100.0f, 100.0f, 100.0f);

		// Create some test entries
		// Cell (0, 0, 0) with trajectory IDs 1, 2
		OutHashTable.Entries.Add(FSpatialHashEntry(
			FSpatialHashTable::CalculateZOrderKey(0, 0, 0), 0, 2));
		OutHashTable.TrajectoryIds.Add(1);
		OutHashTable.TrajectoryIds.Add(2);

		// Cell (1, 0, 0) with trajectory ID 3
		OutHashTable.Entries.Add(FSpatialHashEntry(
			FSpatialHashTable::CalculateZOrderKey(1, 0, 0), 2, 1));
		OutHashTable.TrajectoryIds.Add(3);

		// Cell (0, 1, 0) with trajectory IDs 4, 5, 6
		OutHashTable.Entries.Add(FSpatialHashEntry(
			FSpatialHashTable::CalculateZOrderKey(0, 1, 0), 3, 3));
		OutHashTable.TrajectoryIds.Add(4);
		OutHashTable.TrajectoryIds.Add(5);
		OutHashTable.TrajectoryIds.Add(6);

		// Update header counts
		OutHashTable.Header.NumEntries = OutHashTable.Entries.Num();
		OutHashTable.Header.NumTrajectoryIds = OutHashTable.TrajectoryIds.Num();
	}

	/**
	 * Validate that Z-Order key calculation is correct
	 * 
	 * @return true if all tests pass
	 */
	inline bool ValidateZOrderCalculation()
	{
		// Test basic Z-Order calculation
		uint64 Key000 = FSpatialHashTable::CalculateZOrderKey(0, 0, 0);
		uint64 Key001 = FSpatialHashTable::CalculateZOrderKey(0, 0, 1);
		uint64 Key010 = FSpatialHashTable::CalculateZOrderKey(0, 1, 0);
		uint64 Key100 = FSpatialHashTable::CalculateZOrderKey(1, 0, 0);

		// Verify keys are unique
		if (Key000 == Key001 || Key000 == Key010 || Key000 == Key100 ||
			Key001 == Key010 || Key001 == Key100 || Key010 == Key100)
		{
			UE_LOG(LogTemp, Error, TEXT("Z-Order keys are not unique"));
			return false;
		}

		// Verify (0,0,0) gives 0
		if (Key000 != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Z-Order key for (0,0,0) should be 0, got %llu"), Key000);
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Z-Order calculation validated"));
		return true;
	}

	/**
	 * Validate world to cell coordinate conversion
	 * 
	 * @return true if all tests pass
	 */
	inline bool ValidateWorldToCellConversion()
	{
		FVector BBoxMin(0.0f, 0.0f, 0.0f);
		float CellSize = 10.0f;

		// Test various positions
		int32 X, Y, Z;

		// Position (5, 5, 5) should be in cell (0, 0, 0)
		FSpatialHashTable::WorldToCellCoordinates(FVector(5.0f, 5.0f, 5.0f), BBoxMin, CellSize, X, Y, Z);
		if (X != 0 || Y != 0 || Z != 0)
		{
			UE_LOG(LogTemp, Error, TEXT("Cell conversion failed for (5,5,5): got (%d,%d,%d)"), X, Y, Z);
			return false;
		}

		// Position (15, 25, 35) should be in cell (1, 2, 3)
		FSpatialHashTable::WorldToCellCoordinates(FVector(15.0f, 25.0f, 35.0f), BBoxMin, CellSize, X, Y, Z);
		if (X != 1 || Y != 2 || Z != 3)
		{
			UE_LOG(LogTemp, Error, TEXT("Cell conversion failed for (15,25,35): got (%d,%d,%d)"), X, Y, Z);
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("World to cell conversion validated"));
		return true;
	}

	/**
	 * Validate hash table save and load operations
	 * 
	 * @param TempDirectory Temporary directory for test files
	 * @return true if all tests pass
	 */
	inline bool ValidateSaveAndLoad(const FString& TempDirectory)
	{
		// Create test hash table
		FSpatialHashTable OriginalTable;
		CreateExampleHashTable(OriginalTable);

		// Validate before saving
		if (!OriginalTable.Validate())
		{
			UE_LOG(LogTemp, Error, TEXT("Original hash table validation failed"));
			return false;
		}

		// Save to file
		FString TestFile = FPaths::Combine(TempDirectory, TEXT("test_hashtable.bin"));
		if (!OriginalTable.SaveToFile(TestFile))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to save hash table"));
			return false;
		}

		// Load from file
		FSpatialHashTable LoadedTable;
		if (!LoadedTable.LoadFromFile(TestFile))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load hash table"));
			return false;
		}

		// Validate loaded table
		if (!LoadedTable.Validate())
		{
			UE_LOG(LogTemp, Error, TEXT("Loaded hash table validation failed"));
			return false;
		}

		// Verify data matches
		if (LoadedTable.Header.TimeStep != OriginalTable.Header.TimeStep ||
			LoadedTable.Header.CellSize != OriginalTable.Header.CellSize ||
			LoadedTable.Entries.Num() != OriginalTable.Entries.Num() ||
			LoadedTable.TrajectoryIds.Num() != OriginalTable.TrajectoryIds.Num())
		{
			UE_LOG(LogTemp, Error, TEXT("Loaded hash table data does not match original"));
			return false;
		}

		// Verify entries match
		for (int32 i = 0; i < OriginalTable.Entries.Num(); ++i)
		{
			if (LoadedTable.Entries[i].ZOrderKey != OriginalTable.Entries[i].ZOrderKey ||
				LoadedTable.Entries[i].StartIndex != OriginalTable.Entries[i].StartIndex ||
				LoadedTable.Entries[i].TrajectoryCount != OriginalTable.Entries[i].TrajectoryCount)
			{
				UE_LOG(LogTemp, Error, TEXT("Entry %d does not match"), i);
				return false;
			}
		}

		// Verify trajectory IDs match
		for (int32 i = 0; i < OriginalTable.TrajectoryIds.Num(); ++i)
		{
			if (LoadedTable.TrajectoryIds[i] != OriginalTable.TrajectoryIds[i])
			{
				UE_LOG(LogTemp, Error, TEXT("Trajectory ID %d does not match"), i);
				return false;
			}
		}

		UE_LOG(LogTemp, Log, TEXT("Save and load validation passed"));
		return true;
	}

	/**
	 * Validate hash table query operations
	 * 
	 * @return true if all tests pass
	 */
	inline bool ValidateQuery()
	{
		// Create test hash table
		FSpatialHashTable HashTable;
		CreateExampleHashTable(HashTable);

		// Query at position in cell (0, 0, 0) - should find trajectories 1, 2
		TArray<uint32> TrajectoryIds;
		FVector QueryPos(5.0f, 5.0f, 5.0f);
		
		if (!HashTable.QueryAtPosition(QueryPos, TrajectoryIds))
		{
			UE_LOG(LogTemp, Error, TEXT("Query failed for position in cell (0,0,0)"));
			return false;
		}

		if (TrajectoryIds.Num() != 2 || TrajectoryIds[0] != 1 || TrajectoryIds[1] != 2)
		{
			UE_LOG(LogTemp, Error, TEXT("Query returned incorrect trajectory IDs"));
			return false;
		}

		// Query at position in cell (1, 0, 0) - should find trajectory 3
		TrajectoryIds.Reset();
		QueryPos = FVector(15.0f, 5.0f, 5.0f);
		
		if (!HashTable.QueryAtPosition(QueryPos, TrajectoryIds))
		{
			UE_LOG(LogTemp, Error, TEXT("Query failed for position in cell (1,0,0)"));
			return false;
		}

		if (TrajectoryIds.Num() != 1 || TrajectoryIds[0] != 3)
		{
			UE_LOG(LogTemp, Error, TEXT("Query returned incorrect trajectory IDs for cell (1,0,0)"));
			return false;
		}

		// Query at empty cell - should return false
		TrajectoryIds.Reset();
		QueryPos = FVector(25.0f, 25.0f, 25.0f);
		
		if (HashTable.QueryAtPosition(QueryPos, TrajectoryIds))
		{
			UE_LOG(LogTemp, Error, TEXT("Query should have failed for empty cell"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Query validation passed"));
		return true;
	}

	/**
	 * Validate hash table builder
	 * 
	 * @param TempDirectory Temporary directory for test files
	 * @return true if all tests pass
	 */
	inline bool ValidateBuilder(const FString& TempDirectory)
	{
		// Create test trajectory samples
		TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
		TimeStepSamples.SetNum(2);

		// Time step 0
		TimeStepSamples[0].Add(FSpatialHashTableBuilder::FTrajectorySample(1, FVector(5.0f, 5.0f, 5.0f)));
		TimeStepSamples[0].Add(FSpatialHashTableBuilder::FTrajectorySample(2, FVector(8.0f, 8.0f, 8.0f)));
		TimeStepSamples[0].Add(FSpatialHashTableBuilder::FTrajectorySample(3, FVector(15.0f, 5.0f, 5.0f)));

		// Time step 1
		TimeStepSamples[1].Add(FSpatialHashTableBuilder::FTrajectorySample(1, FVector(6.0f, 6.0f, 6.0f)));
		TimeStepSamples[1].Add(FSpatialHashTableBuilder::FTrajectorySample(2, FVector(9.0f, 9.0f, 9.0f)));

		// Configure builder
		FSpatialHashTableBuilder::FBuildConfig Config;
		Config.CellSize = 10.0f;
		Config.bComputeBoundingBox = true;
		Config.OutputDirectory = TempDirectory;
		Config.NumTimeSteps = 2;

		// Build hash tables
		FSpatialHashTableBuilder Builder;
		if (!Builder.BuildHashTables(Config, TimeStepSamples))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to build hash tables"));
			return false;
		}

		// Verify files were created
		FString File0 = FSpatialHashTableBuilder::GetOutputFilename(TempDirectory, Config.CellSize, 0);
		FString File1 = FSpatialHashTableBuilder::GetOutputFilename(TempDirectory, Config.CellSize, 1);

		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.FileExists(*File0) || !PlatformFile.FileExists(*File1))
		{
			UE_LOG(LogTemp, Error, TEXT("Output files were not created"));
			return false;
		}

		// Load and validate first file
		FSpatialHashTable HashTable0;
		if (!HashTable0.LoadFromFile(File0))
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to load generated hash table 0"));
			return false;
		}

		if (HashTable0.Header.TimeStep != 0 || HashTable0.Header.CellSize != Config.CellSize)
		{
			UE_LOG(LogTemp, Error, TEXT("Generated hash table has incorrect header"));
			return false;
		}

		UE_LOG(LogTemp, Log, TEXT("Builder validation passed"));
		return true;
	}

	/**
	 * Run all validation tests
	 * 
	 * @param TempDirectory Temporary directory for test files
	 * @return true if all tests pass
	 */
	inline bool RunAllValidations(const FString& TempDirectory)
	{
		UE_LOG(LogTemp, Log, TEXT("=== Starting Spatial Hash Table Validation ==="));

		bool bAllPassed = true;

		bAllPassed &= ValidateZOrderCalculation();
		bAllPassed &= ValidateWorldToCellConversion();
		bAllPassed &= ValidateSaveAndLoad(TempDirectory);
		bAllPassed &= ValidateQuery();
		bAllPassed &= ValidateBuilder(TempDirectory);

		if (bAllPassed)
		{
			UE_LOG(LogTemp, Log, TEXT("=== All Validations PASSED ==="));
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("=== Some Validations FAILED ==="));
		}

		return bAllPassed;
	}
}
