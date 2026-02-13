// Copyright Epic Games, Inc. All Rights Reserved.

#include "SpatialHashTable.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"

// Helper function to interleave bits for Z-Order curve calculation
static uint64 SplitBy3(uint32 Value)
{
	// Limit to 21 bits
	uint64 x = Value & 0x1fffff;
	
	// Spread bits to every 3rd position
	x = (x | x << 32) & 0x1f00000000ffff;
	x = (x | x << 16) & 0x1f0000ff0000ff;
	x = (x | x << 8)  & 0x100f00f00f00f00f;
	x = (x | x << 4)  & 0x10c30c30c30c30c3;
	x = (x | x << 2)  & 0x1249249249249249;
	
	return x;
}

uint64 FSpatialHashTable::CalculateZOrderKey(int32 CellX, int32 CellY, int32 CellZ)
{
	// Ensure coordinates are non-negative and within 21-bit range
	uint32 X = FMath::Clamp(CellX, 0, 0x1fffff);
	uint32 Y = FMath::Clamp(CellY, 0, 0x1fffff);
	uint32 Z = FMath::Clamp(CellZ, 0, 0x1fffff);
	
	// Interleave bits: x, y, z, x, y, z, ... (X at bit 0, Y at bit 1, Z at bit 2)
	return SplitBy3(X) | (SplitBy3(Y) << 1) | (SplitBy3(Z) << 2);
}

void FSpatialHashTable::WorldToCellCoordinates(
	const FVector& WorldPos,
	const FVector& BBoxMin,
	float CellSize,
	int32& OutCellX,
	int32& OutCellY,
	int32& OutCellZ)
{
	if (CellSize > SMALL_NUMBER)
	{
		OutCellX = FMath::FloorToInt((WorldPos.X - BBoxMin.X) / CellSize);
		OutCellY = FMath::FloorToInt((WorldPos.Y - BBoxMin.Y) / CellSize);
		OutCellZ = FMath::FloorToInt((WorldPos.Z - BBoxMin.Z) / CellSize);
	}
	else
	{
		OutCellX = 0;
		OutCellY = 0;
		OutCellZ = 0;
	}
}

int32 FSpatialHashTable::FindEntry(uint64 Key) const
{
	// Binary search in sorted entries array
	int32 Left = 0;
	int32 Right = Entries.Num() - 1;
	
	while (Left <= Right)
	{
		int32 Mid = Left + (Right - Left) / 2;
		
		if (Entries[Mid].ZOrderKey == Key)
		{
			return Mid;
		}
		else if (Entries[Mid].ZOrderKey < Key)
		{
			Left = Mid + 1;
		}
		else
		{
			Right = Mid - 1;
		}
	}
	
	return -1; // Not found
}

bool FSpatialHashTable::GetTrajectoryIdsForCell(int32 EntryIndex, TArray<uint32>& OutTrajectoryIds) const
{
	OutTrajectoryIds.Reset();
	
	if (EntryIndex < 0 || EntryIndex >= Entries.Num())
	{
		return false;
	}

	const FSpatialHashEntry& Entry = Entries[EntryIndex];
	
	// If TrajectoryIds array is populated (e.g., for building/saving), use it
	if (TrajectoryIds.Num() > 0)
	{
		// Validate indices
		if (Entry.StartIndex < (uint32)TrajectoryIds.Num() &&
			Entry.StartIndex + Entry.TrajectoryCount <= (uint32)TrajectoryIds.Num())
		{
			OutTrajectoryIds.Reserve(Entry.TrajectoryCount);
			for (uint32 i = 0; i < Entry.TrajectoryCount; ++i)
			{
				OutTrajectoryIds.Add(TrajectoryIds[Entry.StartIndex + i]);
			}
			return true;
		}
		return false;
	}
	
	// Otherwise, read from disk on-demand
	return ReadTrajectoryIdsFromDisk(Entry.StartIndex, Entry.TrajectoryCount, OutTrajectoryIds);
}

bool FSpatialHashTable::QueryAtPosition(const FVector& WorldPos, TArray<uint32>& OutTrajectoryIds) const
{
	OutTrajectoryIds.Reset();
	
	// Convert world position to cell coordinates
	int32 CellX, CellY, CellZ;
	WorldToCellCoordinates(WorldPos, Header.BBoxMin, Header.CellSize, CellX, CellY, CellZ);
	
	// Calculate Z-Order key
	uint64 Key = CalculateZOrderKey(CellX, CellY, CellZ);
	
	// Find entry
	int32 EntryIndex = FindEntry(Key);
	if (EntryIndex >= 0)
	{
		return GetTrajectoryIdsForCell(EntryIndex, OutTrajectoryIds);
	}
	
	return false;
}

bool FSpatialHashTable::SaveToFile(const FString& Filename) const
{
	// Validate before saving
	if (!Validate())
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Validation failed"));
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Create directory if it doesn't exist
	FString Directory = FPaths::GetPath(Filename);
	if (!Directory.IsEmpty() && !PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}

	// Open file for writing
	IFileHandle* FileHandle = PlatformFile.OpenWrite(*Filename);
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Failed to open file for writing: %s"), *Filename);
		return false;
	}

	bool bSuccess = true;

	// Write header
	if (!FileHandle->Write(reinterpret_cast<const uint8*>(&Header), sizeof(FSpatialHashHeader)))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Failed to write header"));
		bSuccess = false;
	}

	// Write hash table entries
	if (bSuccess && Entries.Num() > 0)
	{
		if (!FileHandle->Write(reinterpret_cast<const uint8*>(Entries.GetData()), 
			Entries.Num() * sizeof(FSpatialHashEntry)))
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Failed to write entries"));
			bSuccess = false;
		}
	}

	// Write trajectory IDs
	if (bSuccess && TrajectoryIds.Num() > 0)
	{
		if (!FileHandle->Write(reinterpret_cast<const uint8*>(TrajectoryIds.GetData()), 
			TrajectoryIds.Num() * sizeof(uint32)))
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Failed to write trajectory IDs"));
			bSuccess = false;
		}
	}

	delete FileHandle;

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("FSpatialHashTable::SaveToFile: Successfully saved to %s"), *Filename);
	}

	return bSuccess;
}

bool FSpatialHashTable::LoadFromFile(const FString& Filename)
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Check if file exists
	if (!PlatformFile.FileExists(*Filename))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: File does not exist: %s"), *Filename);
		return false;
	}

	// Store the file path for on-demand loading
	SourceFilePath = Filename;

	// Open file for reading
	IFileHandle* FileHandle = PlatformFile.OpenRead(*Filename);
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Failed to open file for reading: %s"), *Filename);
		return false;
	}

	bool bSuccess = true;

	// Read header
	if (!FileHandle->Read(reinterpret_cast<uint8*>(&Header), sizeof(FSpatialHashHeader)))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Failed to read header"));
		bSuccess = false;
	}

	// Validate header
	if (bSuccess)
	{
		if (Header.Magic != 0x54534854)
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Invalid magic number: 0x%08X"), Header.Magic);
			bSuccess = false;
		}
		else if (Header.Version != 1)
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Unsupported version: %u"), Header.Version);
			bSuccess = false;
		}
	}

	// Read hash table entries
	if (bSuccess && Header.NumEntries > 0)
	{
		Entries.SetNum(Header.NumEntries);
		if (!FileHandle->Read(reinterpret_cast<uint8*>(Entries.GetData()), 
			Header.NumEntries * sizeof(FSpatialHashEntry)))
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Failed to read entries"));
			bSuccess = false;
		}
	}

	// Skip loading trajectory IDs to save memory - they will be read on-demand
	// Note: TrajectoryIds array is already empty from initialization

	delete FileHandle;

	// Validate loaded data
	if (bSuccess && !Validate())
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::LoadFromFile: Validation failed after loading"));
		bSuccess = false;
	}

	if (bSuccess)
	{
		UE_LOG(LogTemp, Log, TEXT("FSpatialHashTable::LoadFromFile: Successfully loaded from %s (trajectory IDs not loaded for memory optimization)"), *Filename);
	}

	return bSuccess;
}

bool FSpatialHashTable::Validate() const
{
	// Check header consistency
	if (Header.Magic != 0x54534854)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Invalid magic number"));
		return false;
	}

	if (Header.Version != 1)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Unsupported version"));
		return false;
	}

	if (Header.CellSize <= 0.0f)
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Invalid cell size"));
		return false;
	}

	if (Header.NumEntries != (uint32)Entries.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Entry count mismatch"));
		return false;
	}

	// Only validate trajectory IDs array if it's populated (e.g., when building/saving)
	if (TrajectoryIds.Num() > 0 && Header.NumTrajectoryIds != (uint32)TrajectoryIds.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Trajectory ID count mismatch"));
		return false;
	}

	// Check that entries are sorted
	for (int32 i = 1; i < Entries.Num(); ++i)
	{
		if (Entries[i].ZOrderKey <= Entries[i - 1].ZOrderKey)
		{
			UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Entries not sorted at index %d"), i);
			return false;
		}
	}

	// Check that all entry indices are valid (only if TrajectoryIds array is populated)
	if (TrajectoryIds.Num() > 0)
	{
		for (const FSpatialHashEntry& Entry : Entries)
		{
			if (Entry.StartIndex >= (uint32)TrajectoryIds.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Invalid start index"));
				return false;
			}

			if (Entry.StartIndex + Entry.TrajectoryCount > (uint32)TrajectoryIds.Num())
			{
				UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Entry exceeds trajectory IDs array"));
				return false;
			}
		}
	}
	else
	{
		// When TrajectoryIds array is empty (on-demand loading mode), validate against header
		for (const FSpatialHashEntry& Entry : Entries)
		{
			if (Entry.StartIndex >= Header.NumTrajectoryIds)
			{
				UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Invalid start index"));
				return false;
			}

			if (Entry.StartIndex + Entry.TrajectoryCount > Header.NumTrajectoryIds)
			{
				UE_LOG(LogTemp, Warning, TEXT("FSpatialHashTable::Validate: Entry exceeds trajectory IDs array"));
				return false;
			}
		}
	}

	return true;
}

bool FSpatialHashTable::ReadTrajectoryIdsFromDisk(uint32 StartIndex, uint32 Count, TArray<uint32>& OutTrajectoryIds) const
{
	OutTrajectoryIds.Reset();

	if (SourceFilePath.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::ReadTrajectoryIdsFromDisk: No source file path set"));
		return false;
	}

	if (Count == 0)
	{
		return true; // Nothing to read
	}

	// Validate indices
	if (StartIndex >= Header.NumTrajectoryIds || StartIndex + Count > Header.NumTrajectoryIds)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::ReadTrajectoryIdsFromDisk: Invalid range [%u, %u) for array size %u"),
			StartIndex, StartIndex + Count, Header.NumTrajectoryIds);
		return false;
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	
	// Open file for reading
	IFileHandle* FileHandle = PlatformFile.OpenRead(*SourceFilePath);
	if (!FileHandle)
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::ReadTrajectoryIdsFromDisk: Failed to open file: %s"), *SourceFilePath);
		return false;
	}

	bool bSuccess = true;

	// Calculate offset to trajectory IDs array
	// File layout: Header (64 bytes) + Entries (NumEntries * 16 bytes) + TrajectoryIds
	int64 TrajectoryIdsOffset = sizeof(FSpatialHashHeader) + (Header.NumEntries * sizeof(FSpatialHashEntry));
	int64 ReadOffset = TrajectoryIdsOffset + (StartIndex * sizeof(uint32));

	// Seek to the correct position
	if (!FileHandle->Seek(ReadOffset))
	{
		UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::ReadTrajectoryIdsFromDisk: Failed to seek to offset %lld"), ReadOffset);
		bSuccess = false;
	}

	// Read the trajectory IDs
	if (bSuccess)
	{
		OutTrajectoryIds.SetNum(Count);
		if (!FileHandle->Read(reinterpret_cast<uint8*>(OutTrajectoryIds.GetData()), Count * sizeof(uint32)))
		{
			UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::ReadTrajectoryIdsFromDisk: Failed to read %u trajectory IDs"), Count);
			bSuccess = false;
		}
	}

	delete FileHandle;

	return bSuccess;
}
