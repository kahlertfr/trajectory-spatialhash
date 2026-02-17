# Binary Format Verification Report

## Overview

This document verifies that the binary file format written by `FSpatialHashTable::SaveToFile()` matches the specification in `specification-spatial-hash-table.md`.

**Note**: Line numbers referenced in this document are accurate as of commit b910383. As the codebase evolves, refer to the specific struct definitions and function implementations rather than exact line numbers.

## Specification Summary

According to `specification-spatial-hash-table.md`, the binary file structure is:

```
+---------------------------+
|     File Header           |  (64 bytes)
+---------------------------+
|     Hash Table Entries    |  (variable size)
+---------------------------+
|     Trajectory IDs Array  |  (variable size)
+---------------------------+
```

## Implementation Analysis

### 1. File Header (64 bytes)

**Specification** (`specification-spatial-hash-table.md` lines 45-63):
```
Offset | Size | Type     | Description
-------|------|----------|--------------------------------------------------
0      | 4    | uint32   | Magic number (0x54534854 = "TSHT")
4      | 4    | uint32   | Version number (current: 1)
8      | 4    | uint32   | Time step index
12     | 4    | float    | Cell size (uniform in all dimensions)
16     | 4    | float    | Bounding box min X
20     | 4    | float    | Bounding box min Y
24     | 4    | float    | Bounding box min Z
28     | 4    | float    | Bounding box max X
32     | 4    | float    | Bounding box max Y
36     | 4    | float    | Bounding box max Z
40     | 4    | uint32   | Number of hash table entries
44     | 4    | uint32   | Total number of trajectory IDs in the array
48     | 16   | -        | Reserved for future use (set to 0)
```

**Implementation** (`SpatialHashTable.h` lines 12-81):
```cpp
struct FSpatialHashHeader
{
    uint32 Magic;          // Offset 0,  Size 4  - 0x54534854 ("TSHT")
    uint32 Version;        // Offset 4,  Size 4  - 1
    uint32 TimeStep;       // Offset 8,  Size 4
    float CellSize;        // Offset 12, Size 4
    float BBoxMinX;        // Offset 16, Size 4
    float BBoxMinY;        // Offset 20, Size 4
    float BBoxMinZ;        // Offset 24, Size 4
    float BBoxMaxX;        // Offset 28, Size 4
    float BBoxMaxY;        // Offset 32, Size 4
    float BBoxMaxZ;        // Offset 36, Size 4
    uint32 NumEntries;     // Offset 40, Size 4
    uint32 NumTrajectoryIds; // Offset 44, Size 4
    uint32 Reserved[4];    // Offset 48, Size 16
};

static_assert(sizeof(FSpatialHashHeader) == 64, "FSpatialHashHeader must be exactly 64 bytes");
```

**Write Code** (`SpatialHashTable.cpp` lines 185-190):
```cpp
// Write header
if (!FileHandle->Write(reinterpret_cast<const uint8*>(&Header), sizeof(FSpatialHashHeader)))
{
    UE_LOG(LogTemp, Error, TEXT("FSpatialHashTable::SaveToFile: Failed to write header"));
    bSuccess = false;
}
```

✅ **MATCHES SPECIFICATION**: The header structure matches exactly.

### 2. Hash Table Entries (16 bytes each)

**Specification** (`specification-spatial-hash-table.md` lines 65-77):
```
Offset | Size | Type     | Description
-------|------|----------|--------------------------------------------------
0      | 8    | uint64   | Z-Order curve key (Morton code) for the cell
8      | 4    | uint32   | Start index in trajectory IDs array
12     | 4    | uint32   | Number of trajectories in this cell

Total entry size: 16 bytes
Entries are sorted by Z-Order key in ascending order for efficient binary search.
```

**Implementation** (`SpatialHashTable.h` lines 90-123):
```cpp
struct FSpatialHashEntry
{
    uint64 ZOrderKey;      // Offset 0,  Size 8
    uint32 StartIndex;     // Offset 8,  Size 4
    uint32 TrajectoryCount; // Offset 12, Size 4
};

static_assert(sizeof(FSpatialHashEntry) == 16, "FSpatialHashEntry must be exactly 16 bytes");
```

**Write Code** (`SpatialHashTable.cpp` lines 192-201):
```cpp
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
```

**Sorting** (`SpatialHashTableBuilder.cpp` lines 166-169):
```cpp
// Sort keys for consistent ordering
TArray<uint64> Keys;
CellMap.GetKeys(Keys);
Keys.Sort();
```

✅ **MATCHES SPECIFICATION**: 
- Entry structure matches exactly (16 bytes)
- Entries are sorted by Z-Order key before writing
- All entries written sequentially

### 3. Trajectory IDs Array (4 bytes each)

**Specification** (`specification-spatial-hash-table.md` lines 79-89):
```
Offset | Size | Type     | Description
-------|------|----------|--------------------------------------------------
0      | 4    | uint32   | Trajectory ID
4      | 4    | uint32   | Trajectory ID
...    | ...  | ...      | ...

The trajectory IDs are grouped by cell, with each cell's IDs stored contiguously.
```

**Implementation**: Trajectory IDs are stored as `TArray<uint32> TrajectoryIds` in `FSpatialHashTable`.

**Write Code** (`SpatialHashTable.cpp` lines 203-212):
```cpp
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
```

**Grouping by Cell** (`SpatialHashTableBuilder.cpp` lines 171-187):
```cpp
// Build entries and trajectory IDs array
uint32 CurrentIndex = 0;
for (uint64 Key : Keys)
{
    const TArray<uint32>& TrajectoryIds = CellMap[Key];
    
    // Create hash entry
    FSpatialHashEntry Entry(Key, CurrentIndex, TrajectoryIds.Num());
    OutHashTable.Entries.Add(Entry);

    // Add trajectory IDs
    for (uint32 TrajectoryId : TrajectoryIds)
    {
        OutHashTable.TrajectoryIds.Add(TrajectoryId);
    }

    CurrentIndex += TrajectoryIds.Num();
}
```

✅ **MATCHES SPECIFICATION**:
- Each trajectory ID is 4 bytes (uint32)
- IDs are grouped by cell in the flat array
- IDs stored contiguously for each cell

## File Layout Verification

**Specification**:
```
File Layout:
  [Header: 64 bytes]
  [Entries: NumEntries × 16 bytes, sorted by ZOrderKey]
  [Trajectory IDs: NumTrajectoryIds × 4 bytes]
```

**Implementation** (`SpatialHashTable.cpp` lines 185-212):
```cpp
// 1. Write header (64 bytes)
FileHandle->Write(&Header, sizeof(FSpatialHashHeader))

// 2. Write entries (NumEntries × 16 bytes)
FileHandle->Write(Entries.GetData(), Entries.Num() * sizeof(FSpatialHashEntry))

// 3. Write trajectory IDs (NumTrajectoryIds × 4 bytes)
FileHandle->Write(TrajectoryIds.GetData(), TrajectoryIds.Num() * sizeof(uint32))
```

✅ **MATCHES SPECIFICATION**: Exact order and sizes match.

## Z-Order Key Implementation

**Specification** (`specification-spatial-hash-table.md` lines 92-109):
```
1. Take the lower 21 bits of each coordinate (supports coordinates up to 2,097,151)
2. Interleave the bits to produce the pattern: x₀, y₀, z₀, x₁, y₁, z₁, ...
   where xᵢ is the i-th bit of cx (x₀ is bit 0, the least significant bit)
   - Bit 0 (LSB): x₀ (bit 0 of cx)
   - Bit 1: y₀ (bit 0 of cy)
   - Bit 2: z₀ (bit 0 of cz)
   - Bit 3: x₁ (bit 1 of cx)
   - Bit 4: y₁ (bit 1 of cy)
   - Bit 5: z₁ (bit 1 of cz)
   - ... and so on
3. The resulting 63-bit value is the Z-Order key
```

**Implementation** (`SpatialHashTable.cpp` lines 24-50):
```cpp
uint64 FSpatialHashTable::CalculateZOrderKey(int32 CellX, int32 CellY, int32 CellZ)
{
    // Ensure coordinates are non-negative and within 21-bit range
    uint32 X = FMath::Clamp(CellX, 0, 0x1fffff);
    uint32 Y = FMath::Clamp(CellY, 0, 0x1fffff);
    uint32 Z = FMath::Clamp(CellZ, 0, 0x1fffff);
    
    // Interleave bits: x at bit 0, y at bit 1, z at bit 2, repeating
    return SplitBy3(X) | (SplitBy3(Y) << 1) | (SplitBy3(Z) << 2);
}
```

✅ **MATCHES SPECIFICATION**: 
- Uses 21 bits per coordinate
- Interleaves bits in pattern x₀, y₀, z₀, x₁, y₁, z₁, ...
- Produces 63-bit Morton code

## Conclusion

### ✅ VERIFICATION RESULT: **PASS**

The binary format written by `FSpatialHashTable::SaveToFile()` **EXACTLY MATCHES** the specification in `specification-spatial-hash-table.md`.

### Summary Table

| Component | Specification | Implementation | Match |
|-----------|--------------|----------------|-------|
| Header Size | 64 bytes | 64 bytes | ✅ |
| Header Magic | 0x54534854 | 0x54534854 | ✅ |
| Header Version | 1 | 1 | ✅ |
| Header Fields | 12 fields + reserved | 12 fields + reserved | ✅ |
| Entry Size | 16 bytes | 16 bytes | ✅ |
| Entry Fields | ZOrderKey, StartIndex, Count | ZOrderKey, StartIndex, TrajectoryCount | ✅ |
| Entry Sorting | Sorted by Z-Order key | Keys.Sort() before build | ✅ |
| Trajectory ID Size | 4 bytes (uint32) | 4 bytes (uint32) | ✅ |
| Trajectory ID Grouping | By cell, contiguous | By cell via loop | ✅ |
| File Order | Header → Entries → IDs | Header → Entries → IDs | ✅ |
| Z-Order Algorithm | 21-bit interleaving | 21-bit interleaving | ✅ |

### Testing

To verify a binary file matches the specification, you can:

1. **Use the verification tool**:
   ```bash
   g++ -std=c++11 verify_binary_format.cpp -o verify_binary_format
   ./verify_binary_format /path/to/timestep_00000.bin
   ```

2. **Check with hexdump**:
   ```bash
   hexdump -C timestep_00000.bin | head -20
   # Should see "TSHT" (0x54534854) at offset 0
   # Should see version 1 at offset 4
   ```

3. **Use the existing validation**:
   The `FSpatialHashTable::Validate()` method already checks:
   - Magic number is 0x54534854
   - Version is 1
   - Entries are sorted by Z-Order key
   - Index ranges are valid

### References

- **Specification**: `specification-spatial-hash-table.md`
- **Header Structure**: `Source/SpatialHashedTrajectory/Public/SpatialHashTable.h` lines 12-84
- **Entry Structure**: `Source/SpatialHashedTrajectory/Public/SpatialHashTable.h` lines 90-123
- **Write Implementation**: `Source/SpatialHashedTrajectory/Private/SpatialHashTable.cpp` lines 157-222
- **Build Implementation**: `Source/SpatialHashedTrajectory/Private/SpatialHashTableBuilder.cpp` lines 120-195
