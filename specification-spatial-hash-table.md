# Spatial Hash Table Binary Format Specification

## Overview

This document describes the binary file format for spatial hash tables used to enable efficient fixed-radius nearest neighbor searches on trajectory data. Each hash table represents a single time step and maps spatial cells (identified by Z-Order curve keys) to trajectory IDs.

## File Organization

Spatial hash table files are organized in the following directory structure:

```
<dataset-directory>/
└── spatial_hashing/
    ├── cellsize_<X.XXX>/
    │   ├── timestep_00000.bin
    │   ├── timestep_00001.bin
    │   └── ...
    ├── cellsize_<Y.YYY>/
    │   ├── timestep_00000.bin
    │   └── ...
    └── ...
```

Where:
- `<dataset-directory>` is the root directory containing trajectory data
- `cellsize_<X.XXX>` represents the cell size (e.g., `cellsize_10.000` for 10 unit cells)
- `timestep_XXXXX.bin` represents the hash table for a specific time step

## Binary File Format

Each `.bin` file contains a complete spatial hash table for one time step and can be memory-mapped directly. The file is structured as follows:

### File Structure

```
+---------------------------+
|     File Header           |  (64 bytes)
+---------------------------+
|     Hash Table Entries    |  (variable size)
+---------------------------+
|     Trajectory IDs Array  |  (variable size)
+---------------------------+
```

### File Header (64 bytes)

The header contains metadata required to interpret and use the hash table:

| Offset | Size | Type     | Description                                           |
|--------|------|----------|-------------------------------------------------------|
| 0      | 4    | uint32   | Magic number (0x54534854 = "TSHT" = Trajectory Spatial Hash Table) |
| 4      | 4    | uint32   | Version number (current: 1)                          |
| 8      | 4    | uint32   | Time step index                                      |
| 12     | 4    | float    | Cell size (uniform in all dimensions)                |
| 16     | 4    | float    | Bounding box min X                                   |
| 20     | 4    | float    | Bounding box min Y                                   |
| 24     | 4    | float    | Bounding box min Z                                   |
| 28     | 4    | float    | Bounding box max X                                   |
| 32     | 4    | float    | Bounding box max Y                                   |
| 36     | 4    | float    | Bounding box max Z                                   |
| 40     | 4    | uint32   | Number of hash table entries                         |
| 44     | 4    | uint32   | Total number of trajectory IDs in the array          |
| 48     | 16   | -        | Reserved for future use (set to 0)                   |

### Hash Table Entries

Following the header, the hash table entries are stored as an array of fixed-size records. Each entry represents one occupied spatial cell:

| Offset | Size | Type     | Description                                           |
|--------|------|----------|-------------------------------------------------------|
| 0      | 8    | uint64   | Z-Order curve key (Morton code) for the cell         |
| 8      | 4    | uint32   | Start index in trajectory IDs array                  |
| 12     | 4    | uint32   | Number of trajectories in this cell                  |

**Total entry size: 16 bytes**

Entries are sorted by Z-Order key in ascending order for efficient binary search.

### Trajectory IDs Array

After all hash table entries, trajectory IDs are stored sequentially as an array of unsigned 32-bit integers:

| Offset | Size | Type     | Description                                           |
|--------|------|----------|-------------------------------------------------------|
| 0      | 4    | uint32   | Trajectory ID                                        |
| 4      | 4    | uint32   | Trajectory ID                                        |
| ...    | ...  | ...      | ...                                                  |

The trajectory IDs are grouped by cell, with each cell's IDs stored contiguously.

## Z-Order Curve (Morton Code)

The Z-Order curve maps 3D spatial coordinates to a single 64-bit integer key. This provides good spatial locality properties for hash table lookups.

### Calculation

Given a 3D cell coordinate (cx, cy, cz) where each component is a non-negative integer:

1. Take the lower 21 bits of each coordinate (supports coordinates up to 2,097,151)
2. Interleave the bits to produce the pattern: x₀, y₀, z₀, x₁, y₁, z₁, ..., where xᵢ is the i-th bit of cx (x₀ is bit 0, the least significant bit)
   - Bit 0 (LSB): x₀ (bit 0 of cx)
   - Bit 1: y₀ (bit 0 of cy)
   - Bit 2: z₀ (bit 0 of cz)
   - Bit 3: x₁ (bit 1 of cx)
   - Bit 4: y₁ (bit 1 of cy)
   - Bit 5: z₁ (bit 1 of cz)
   - ... and so on
3. The resulting 63-bit value is the Z-Order key

### Cell Coordinate Calculation

To convert a 3D world position (x, y, z) to cell coordinates:

```
cx = floor((x - bbox_min_x) / cell_size)
cy = floor((y - bbox_min_y) / cell_size)
cz = floor((z - bbox_min_z) / cell_size)
```

## Usage Example

### Loading a Hash Table (Memory-Optimized)

```cpp
// 1. Open the file
FILE* file = fopen("timestep_00000.bin", "rb");

// 2. Read and validate header
SpatialHashHeader header;
fread(&header, sizeof(header), 1, file);
assert(header.magic == 0x54534854);
assert(header.version == 1);

// 3. Read hash table entries
HashEntry* entries = new HashEntry[header.num_entries];
fread(entries, sizeof(HashEntry), header.num_entries, file);

// 4. Trajectory IDs are NOT loaded into memory
// Close file - will reopen for on-demand queries
fclose(file);

// Store file offset to trajectory IDs for later use
int64_t trajectory_ids_offset = sizeof(SpatialHashHeader) + 
                                (header.num_entries * sizeof(HashEntry));
```

### Querying Trajectories in a Cell (On-Demand Loading)

```cpp
// 1. Calculate cell coordinates from world position
int cx = floor((query_x - header.bbox_min_x) / header.cell_size);
int cy = floor((query_y - header.bbox_min_y) / header.cell_size);
int cz = floor((query_z - header.bbox_min_z) / header.cell_size);

// 2. Calculate Z-Order key
uint64_t key = CalculateZOrderKey(cx, cy, cz);

// 3. Binary search in hash table entries
int entry_index = BinarySearch(entries, header.num_entries, key);

if (entry_index >= 0) {
    // 4. Read trajectory IDs from disk on-demand
    uint32_t start_idx = entries[entry_index].start_index;
    uint32_t count = entries[entry_index].trajectory_count;
    
    // Open file for reading
    FILE* file = fopen("timestep_00000.bin", "rb");
    
    // Seek to trajectory IDs location
    int64_t offset = trajectory_ids_offset + (start_idx * sizeof(uint32_t));
    fseek(file, offset, SEEK_SET);
    
    // Read only the IDs we need
    uint32_t* trajectory_ids = new uint32_t[count];
    fread(trajectory_ids, sizeof(uint32_t), count, file);
    
    // Close file immediately after reading
    fclose(file);
    
    // Process trajectory IDs
    for (uint32_t i = 0; i < count; i++) {
        uint32_t trajectory_id = trajectory_ids[i];
        // Process trajectory_id...
    }
    
    // Clean up
    delete[] trajectory_ids;
}
```

## Design Rationale

### Memory Optimization Strategy

The plugin implements an on-demand loading strategy to minimize memory usage:

**What's Loaded into Memory:**
- **Header** (64 bytes): Always loaded - contains metadata needed for queries
- **Hash Table Entries** (16 bytes each): Always loaded - needed for binary search to locate cells

**What's NOT Loaded into Memory:**
- **Trajectory IDs Array**: Read from disk on-demand when a cell is queried
- **No Caching**: IDs are re-read from disk for each query to maintain minimal memory footprint

**Memory Savings Example:**
- Hash table with 100,000 entries and 1,000,000 trajectory IDs
- In-memory: 64 bytes + (100,000 × 16 bytes) = ~1.6 MB
- Without optimization: 64 bytes + (100,000 × 16 bytes) + (1,000,000 × 4 bytes) = ~5.6 MB
- **Savings: 4 MB per hash table** (71% reduction)

**Scalability:**
With 100 time steps loaded, this design saves ~400 MB of memory while maintaining fast query performance through binary search and efficient file I/O.

### Memory-Mappable Format

The format is designed to be loaded via memory mapping (mmap) or simple file read operations without parsing:
- Fixed-size header with all metadata
- Fixed-size hash table entries
- Contiguous arrays suitable for direct memory access
- No variable-length fields or pointers

### Z-Order Curve Benefits

- **Spatial locality**: Nearby cells in 3D space have similar Z-Order keys
- **Range queries**: Can efficiently query rectangular regions
- **Single key**: Reduces hash table complexity compared to 3D coordinate keys
- **Sortable**: Enables binary search in sorted array

### Separate Trajectory IDs Array

Rather than embedding trajectory IDs in each hash entry:
- Supports variable number of trajectories per cell
- Reduces memory overhead for sparse cells
- Enables efficient sequential access to all IDs in a cell
- Simplifies memory layout for direct loading

## Version History

- **Version 1** (current): Initial format specification
  - Basic header with bounding box and metadata
  - Z-Order based hash table entries
  - Separate trajectory IDs array

## Future Considerations

Potential enhancements for future versions:
- Compressed formats for very large datasets
- Multi-resolution support
- Additional spatial indexing structures
- Trajectory metadata (velocity, acceleration) per entry
