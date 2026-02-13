// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * File header for spatial hash table binary files
 * Total size: 64 bytes
 */
struct FSpatialHashHeader
{
	/** Magic number for file identification: 0x54534854 ("TSHT") */
	uint32 Magic;
	
	/** Format version number (current: 1) */
	uint32 Version;
	
	/** Time step index this hash table represents */
	uint32 TimeStep;
	
	/** Cell size in world units (uniform in all dimensions) */
	float CellSize;
	
	/** Bounding box minimum coordinates */
	FVector BBoxMin;
	
	/** Bounding box maximum coordinates */
	FVector BBoxMax;
	
	/** Number of entries in the hash table */
	uint32 NumEntries;
	
	/** Total number of trajectory IDs in the trajectory IDs array */
	uint32 NumTrajectoryIds;
	
	/** Reserved bytes for future use (16 bytes) */
	uint32 Reserved[4];

	FSpatialHashHeader()
		: Magic(0x54534854) // "TSHT"
		, Version(1)
		, TimeStep(0)
		, CellSize(1.0f)
		, BBoxMin(FVector::ZeroVector)
		, BBoxMax(FVector::ZeroVector)
		, NumEntries(0)
		, NumTrajectoryIds(0)
	{
		FMemory::Memzero(Reserved, sizeof(Reserved));
	}
};

// Ensure the header is exactly 64 bytes
static_assert(sizeof(FSpatialHashHeader) == 64, "FSpatialHashHeader must be exactly 64 bytes");

/**
 * Hash table entry representing a single spatial cell
 * Total size: 16 bytes
 */
struct FSpatialHashEntry
{
	/** Z-Order curve key (Morton code) for this cell */
	uint64 ZOrderKey;
	
	/** Start index in the trajectory IDs array */
	uint32 StartIndex;
	
	/** Number of trajectories in this cell */
	uint32 TrajectoryCount;

	FSpatialHashEntry()
		: ZOrderKey(0)
		, StartIndex(0)
		, TrajectoryCount(0)
	{
	}

	FSpatialHashEntry(uint64 InKey, uint32 InStartIndex, uint32 InCount)
		: ZOrderKey(InKey)
		, StartIndex(InStartIndex)
		, TrajectoryCount(InCount)
	{
	}

	/** Comparison operator for sorting by Z-Order key */
	bool operator<(const FSpatialHashEntry& Other) const
	{
		return ZOrderKey < Other.ZOrderKey;
	}
};

// Ensure the entry is exactly 16 bytes
static_assert(sizeof(FSpatialHashEntry) == 16, "FSpatialHashEntry must be exactly 16 bytes");

/**
 * In-memory representation of a spatial hash table for one time step
 * 
 * Note: To optimize memory usage, trajectory IDs are not loaded into memory.
 * Instead, they are read on-demand from disk when queried.
 */
class SPATIALHASHEDTRAJECTORY_API FSpatialHashTable
{
public:
	/** Header information */
	FSpatialHashHeader Header;
	
	/** Sorted array of hash table entries */
	TArray<FSpatialHashEntry> Entries;
	
	/** Array of trajectory IDs, grouped by cell (used for building/saving only) */
	TArray<uint32> TrajectoryIds;
	
	/** Path to the source file for on-demand trajectory ID loading */
	FString SourceFilePath;

	FSpatialHashTable() = default;

	/**
	 * Calculate Z-Order key (Morton code) from 3D cell coordinates
	 * @param CellX X cell coordinate
	 * @param CellY Y cell coordinate
	 * @param CellZ Z cell coordinate
	 * @return 64-bit Z-Order key
	 */
	static uint64 CalculateZOrderKey(int32 CellX, int32 CellY, int32 CellZ);

	/**
	 * Convert world position to cell coordinates
	 * @param WorldPos World space position
	 * @param BBoxMin Bounding box minimum
	 * @param CellSize Cell size in world units
	 * @param OutCellX Output X cell coordinate
	 * @param OutCellY Output Y cell coordinate
	 * @param OutCellZ Output Z cell coordinate
	 */
	static void WorldToCellCoordinates(
		const FVector& WorldPos,
		const FVector& BBoxMin,
		float CellSize,
		int32& OutCellX,
		int32& OutCellY,
		int32& OutCellZ);

	/**
	 * Find hash entry by Z-Order key using binary search
	 * @param Key Z-Order key to search for
	 * @return Index of entry if found, -1 otherwise
	 */
	int32 FindEntry(uint64 Key) const;

	/**
	 * Get trajectory IDs for a specific cell (reads from disk on-demand)
	 * @param EntryIndex Index of the hash table entry
	 * @param OutTrajectoryIds Output array of trajectory IDs
	 * @return true if successful, false otherwise
	 */
	bool GetTrajectoryIdsForCell(int32 EntryIndex, TArray<uint32>& OutTrajectoryIds) const;

	/**
	 * Query trajectory IDs at a specific world position (reads from disk on-demand)
	 * @param WorldPos World space position
	 * @param OutTrajectoryIds Output array of trajectory IDs
	 * @return true if cell was found, false otherwise
	 */
	bool QueryAtPosition(const FVector& WorldPos, TArray<uint32>& OutTrajectoryIds) const;

	/**
	 * Save hash table to binary file
	 * @param Filename Path to output file
	 * @return true if successful, false otherwise
	 */
	bool SaveToFile(const FString& Filename) const;

	/**
	 * Load hash table from binary file (trajectory IDs not loaded into memory)
	 * @param Filename Path to input file
	 * @return true if successful, false otherwise
	 */
	bool LoadFromFile(const FString& Filename);

	/**
	 * Validate the hash table structure
	 * @return true if valid, false otherwise
	 */
	bool Validate() const;

private:
	/**
	 * Read trajectory IDs from disk for a specific range
	 * @param StartIndex Starting index in the trajectory IDs array
	 * @param Count Number of IDs to read
	 * @param OutTrajectoryIds Output array of trajectory IDs
	 * @return true if successful, false otherwise
	 */
	bool ReadTrajectoryIdsFromDisk(uint32 StartIndex, uint32 Count, TArray<uint32>& OutTrajectoryIds) const;
};
