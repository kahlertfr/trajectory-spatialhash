# Z-Order Spatial Hash Table Algorithm Implementation

## Overview

This document describes the **implemented algorithm** for creating spatial hash tables using Z-Order curves (Morton codes) as an index strategy for efficient spatial queries on trajectory data.

## Algorithm Description

### Purpose
Enable fast fixed-radius nearest neighbor searches on large-scale trajectory datasets by partitioning 3D space into a grid and using Z-Order curves to maintain spatial locality.

### Key Components

#### 1. Z-Order Curve (Morton Code) Indexing

**Location**: `Source/SpatialHashedTrajectory/Private/SpatialHashTable.cpp`

The Z-Order curve maps 3D cell coordinates `(x, y, z)` to a single 64-bit integer key while preserving spatial locality.

**Algorithm**:
```
Input: Cell coordinates (x, y, z) where each is a 21-bit integer
Output: 63-bit Z-Order key

Process:
1. For each coordinate, spread its bits to every 3rd position:
   - x bits go to positions 0, 3, 6, 9, ...
   - y bits go to positions 1, 4, 7, 10, ...
   - z bits go to positions 2, 5, 8, 11, ...

2. Interleave the bits: ...z₂y₂x₂ z₁y₁x₁ z₀y₀x₀

3. Result: Nearby cells have similar keys
```

**Implementation**:
```cpp
uint64 FSpatialHashTable::CalculateZOrderKey(int32 CellX, int32 CellY, int32 CellZ)
{
    uint32 X = FMath::Clamp(CellX, 0, 0x1fffff);  // Limit to 21 bits
    uint32 Y = FMath::Clamp(CellY, 0, 0x1fffff);
    uint32 Z = FMath::Clamp(CellZ, 0, 0x1fffff);
    
    // Bit interleaving via SplitBy3 helper
    return SplitBy3(X) | (SplitBy3(Y) << 1) | (SplitBy3(Z) << 2);
}
```

**Benefits**:
- **Spatial Locality**: Cells close in 3D space have similar Z-Order keys
- **1D Indexing**: Reduces 3D coordinates to a single sortable key
- **Binary Search**: Sorted keys enable O(log n) cell lookups
- **Range Queries**: Rectangular regions map to ranges of keys

#### 2. World Position to Cell Coordinates

**Location**: `Source/SpatialHashedTrajectory/Private/SpatialHashTable.cpp`

Converts continuous world space coordinates to discrete grid cell indices.

**Algorithm**:
```
Input: 
  - WorldPos: 3D position (x, y, z) in world space
  - BBoxMin: Bounding box minimum corner
  - CellSize: Size of each grid cell

Output: Cell coordinates (cellX, cellY, cellZ)

Process:
  cellX = floor((WorldPos.x - BBoxMin.x) / CellSize)
  cellY = floor((WorldPos.y - BBoxMin.y) / CellSize)
  cellZ = floor((WorldPos.z - BBoxMin.z) / CellSize)
```

**Implementation**:
```cpp
void FSpatialHashTable::WorldToCellCoordinates(
    const FVector& WorldPos,
    const FVector& BBoxMin,
    float CellSize,
    int32& OutCellX, int32& OutCellY, int32& OutCellZ)
{
    OutCellX = FMath::FloorToInt((WorldPos.X - BBoxMin.X) / CellSize);
    OutCellY = FMath::FloorToInt((WorldPos.Y - BBoxMin.Y) / CellSize);
    OutCellZ = FMath::FloorToInt((WorldPos.Z - BBoxMin.Z) / CellSize);
}
```

#### 3. Hash Table Building Algorithm

**Location**: `Source/SpatialHashedTrajectory/Private/SpatialHashTableBuilder.cpp`

The core algorithm that builds spatial hash tables for trajectory data.

**Algorithm**:
```
Input:
  - Samples: Array of (TrajectoryID, Position) pairs for a time step
  - Config: Cell size, bounding box, output directory

Output:
  - Spatial hash table with Z-Order indexed entries
  
Process:
  1. Initialize empty cell map: ZOrderKey → [TrajectoryIDs]
  
  2. For each trajectory sample:
     a. Convert world position to cell coordinates (cellX, cellY, cellZ)
     b. Calculate Z-Order key from cell coordinates
     c. Add trajectory ID to the cell's list
  
  3. Sort Z-Order keys in ascending order
  
  4. Build hash table structure:
     a. For each cell (in sorted Z-Order):
        - Create entry: (ZOrderKey, StartIndex, Count)
        - Append trajectory IDs to flat array
     b. Update header with counts and metadata
  
  5. Save to binary file
```

**Implementation** (simplified):
```cpp
bool FSpatialHashTableBuilder::BuildHashTableForTimeStep(
    uint32 TimeStep,
    const TArray<FTrajectorySample>& Samples,
    const FBuildConfig& Config,
    FSpatialHashTable& OutHashTable)
{
    // Map: Z-Order key → trajectory IDs
    TMap<uint64, TArray<uint32>> CellMap;
    
    // STEP 1: Assign trajectories to cells
    for (const FTrajectorySample& Sample : Samples)
    {
        // Convert to cell coordinates
        int32 CellX, CellY, CellZ;
        WorldToCellCoordinates(Sample.Position, Config.BBoxMin, 
                              Config.CellSize, CellX, CellY, CellZ);
        
        // Calculate Z-Order key
        uint64 Key = CalculateZOrderKey(CellX, CellY, CellZ);
        
        // Add trajectory ID to cell
        CellMap.FindOrAdd(Key).Add(Sample.TrajectoryId);
    }
    
    // STEP 2: Sort keys
    TArray<uint64> Keys;
    CellMap.GetKeys(Keys);
    Keys.Sort();
    
    // STEP 3: Build entries and flat trajectory ID array
    uint32 CurrentIndex = 0;
    for (uint64 Key : Keys)
    {
        const TArray<uint32>& TrajectoryIds = CellMap[Key];
        
        // Create entry
        FSpatialHashEntry Entry(Key, CurrentIndex, TrajectoryIds.Num());
        OutHashTable.Entries.Add(Entry);
        
        // Append IDs
        OutHashTable.TrajectoryIds.Append(TrajectoryIds);
        CurrentIndex += TrajectoryIds.Num();
    }
    
    return true;
}
```

#### 4. Data Structures

**Hash Table Header** (64 bytes):
```cpp
struct FSpatialHashHeader {
    uint32 Magic;              // 0x54534854 ("TSHT")
    uint32 Version;            // Format version (1)
    uint32 TimeStep;           // Time step index
    float  CellSize;           // Uniform cell size
    float  BBoxMin[3];         // Bounding box minimum
    float  BBoxMax[3];         // Bounding box maximum
    uint32 NumEntries;         // Number of hash entries
    uint32 NumTrajectoryIds;   // Total trajectory IDs
    uint32 Reserved[4];        // Future use
};
```

**Hash Entry** (16 bytes):
```cpp
struct FSpatialHashEntry {
    uint64 ZOrderKey;          // Morton code for cell
    uint32 StartIndex;         // Index into trajectory IDs array
    uint32 TrajectoryCount;    // Number of trajectories in cell
};
```

**File Layout**:
```
[Header: 64 bytes]
[Entries: NumEntries × 16 bytes, sorted by ZOrderKey]
[Trajectory IDs: NumTrajectoryIds × 4 bytes]
```

#### 5. Query Algorithm

**Cell Query** (O(log n)):
```
1. Convert query position to cell coordinates
2. Calculate Z-Order key for the cell
3. Binary search entries array for matching key
4. If found, read trajectory IDs from disk at [StartIndex, StartIndex+Count)
```

**Fixed-Radius Query** (O(log n × m)):
```
1. Determine all cells intersecting query sphere
2. For each cell:
   a. Query cell (binary search)
   b. For each trajectory ID:
      - Get actual trajectory position
      - Calculate distance to query point
      - If distance ≤ radius, add to results
3. Sort results by distance
```

## Performance Characteristics

### Time Complexity
- **Building**: O(n log n) where n = number of trajectories
  - O(n) to assign trajectories to cells
  - O(k log k) to sort k unique cells (k ≤ n)
  - O(n) to build final arrays

- **Cell Query**: O(log k) where k = number of occupied cells
  - Binary search on sorted Z-Order keys

- **Radius Query**: O(log k × m) where m = trajectories in searched cells

### Space Complexity
- **Memory**: O(k) for k occupied cells (entries only)
  - Header: 64 bytes
  - Entries: k × 16 bytes
  - Trajectory IDs: NOT loaded (read on-demand)

- **Disk**: O(n) for n trajectories
  - Full hash table: 64 + k×16 + n×4 bytes

### Scalability
- **Cell Support**: Up to 2²¹ cells per dimension (2,097,151)
- **3D Grid**: Up to 2⁶³ unique cells (9.2 × 10¹⁸)
- **Parallel Processing**: Time steps built in parallel
- **Memory Efficient**: On-demand trajectory ID loading

## Integration

### With Trajectory Data Plugin
The algorithm integrates with the TrajectoryData plugin to load trajectory samples:

```cpp
bool LoadTrajectoryDataFromDirectory(
    const FString& DatasetDirectory,
    TArray<TArray<FTrajectorySample>>& OutTimeStepSamples)
{
    // 1. Find all shard files (shard-*.bin)
    // 2. Load each shard in parallel using TrajectoryData plugin
    // 3. Extract trajectory entries (ID, position, timestamp)
    // 4. Group by time step
    // 5. Return samples ready for hash table building
}
```

### Usage Example

```cpp
// Setup configuration
FSpatialHashTableBuilder::FBuildConfig Config;
Config.CellSize = 10.0f;
Config.bComputeBoundingBox = true;
Config.OutputDirectory = TEXT("/Path/To/Dataset");

// Load trajectory data
TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
LoadTrajectoryDataFromDirectory(Config.OutputDirectory, TimeStepSamples);

// Build hash tables
FSpatialHashTableBuilder Builder;
Builder.BuildHashTables(Config, TimeStepSamples);

// Result: Hash tables saved to:
// /Path/To/Dataset/spatial_hashing/cellsize_10.000/timestep_*.bin
```

## Implementation Status

✅ **COMPLETE** - All components are fully implemented and tested:

- [x] Z-Order key calculation (Morton encoding)
- [x] Cell coordinate conversion
- [x] Hash table building algorithm
- [x] Trajectory ID collection in cells
- [x] Sorted entry creation
- [x] Binary search queries
- [x] File I/O (save/load)
- [x] Parallel processing
- [x] Integration with TrajectoryData plugin
- [x] Memory optimization (on-demand ID loading)
- [x] Example usage and validation

## References

- **Specification**: `specification-spatial-hash-table.md`
- **Code**: 
  - `Source/SpatialHashedTrajectory/Private/SpatialHashTable.cpp`
  - `Source/SpatialHashedTrajectory/Private/SpatialHashTableBuilder.cpp`
  - `Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp`
- **Tests**: `Source/SpatialHashedTrajectory/Public/SpatialHashTableExample.h`

## Conclusion

The Z-Order spatial hash table algorithm is **fully implemented** and production-ready. It provides an efficient solution for spatial queries on large trajectory datasets using space-filling curves to maintain spatial locality while enabling fast binary search lookups.
