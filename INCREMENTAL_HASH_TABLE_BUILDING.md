# Per-Timestep Hash Table Building

## Problem Statement

> "load a batch of trajectory data from the shard files and write the corresponding hash table files for these files. The previous loaded batch can now be freed completely since the data is not relevant anymore for the next time steps. So don't accumulate samples. One shard file contains multiple timesteps. A hash table is built for each timestep. So after reading one shard file multiple hash table files are created (one for each timestep). The hash tables are then finished and do not need to be touched again. So parallelizing based on timesteps should be easy since one thread can focus on creating one hash table."

## Solution: Batch Processing with Per-Timestep Hash Table Building

Process shards in batches, building and writing hash tables for each timestep immediately:

```
For each batch of shards:
  1. Load batch shards (e.g., 3 shard files)
  2. Extract samples, organized by timestep
  3. For each timestep in parallel:
     - Build hash table from samples
     - Write to disk
  4. Free all batch data
  5. Next batch
```

## Key Architecture

### Shard Structure

**One shard file contains multiple timesteps:**
- Example: `shard-3000.bin` contains timesteps 3000-3099 (100 timesteps)
- Each trajectory has position data for each timestep
- Each timestep needs its own independent hash table file

### Hash Table Independence

**Each timestep = One independent hash table:**
- `timestep_3000.bin` - hash table for timestep 3000
- `timestep_3001.bin` - hash table for timestep 3001
- ...
- `timestep_3099.bin` - hash table for timestep 3099

Once a hash table is written, it's complete and never modified.

### No Accumulation

**Each batch is self-contained:**
- Batch 1 (shards 0-2): Build hash tables for all timesteps in these shards → Write → Free
- Batch 2 (shards 3-5): Build hash tables for all timesteps in these shards → Write → Free
- No data carried over between batches

## Implementation

### Method: BuildHashTablesIncrementallyFromShards()

**Pass 1: Scan for metadata**
- Determine global time range
- Compute bounding box
- Store shard starting timesteps

**Pass 2: Batch processing**
```cpp
for each batch of shards:
    // 1. Load batch
    Load 3 shard files → BatchShardData
    
    // 2. Determine batch timestep range
    BatchMinTimeStep, BatchMaxTimeStep from loaded shards
    
    // 3. Extract samples by timestep
    TArray<TArray<FSample>> BatchTimeStepSamples[BatchTimeSteps]
    
    ParallelFor(shards in batch):
        for each trajectory in shard:
            for each timestep in trajectory:
                Add sample to BatchTimeStepSamples[timestep]
    
    // Free shard data
    BatchShardData.Empty()
    
    // 4. Build hash tables in parallel
    ParallelFor(timestep in batch):
        HashTable = BuildHashTableForTimeStep(timestep, samples)
        HashTable.SaveToFile(filename)
    
    // 5. Free all batch data
    BatchTimeStepSamples.Empty()
```

### Parallelization

**Two levels of parallelism:**

1. **Sample extraction** (within batch):
   - Process shards in parallel
   - Extract samples from each shard concurrently
   - Thread-safe accumulation into BatchTimeStepSamples

2. **Hash table building** (within batch):
   - Build one hash table per thread
   - Each timestep processed independently
   - Write to disk in parallel (different files)

### Memory Usage

**Per batch:**
```
Load shards:           3 shards × 2GB = 6GB
Extract samples:       100 timesteps × 50MB = 5GB
Free shards:           -6GB
Build hash tables:     N_threads × 200MB = 1GB (parallel)
Free samples:          -5GB

Peak: 11GB per batch (constant regardless of total shards)
```

**Example with 50 shards:**
```
Old approach (accumulation):
  Load all: 100GB
  Build all: 100GB + 5GB = 105GB peak

New approach (batch):
  Batch 1: 11GB → free
  Batch 2: 11GB → free
  ...
  Batch 17: 11GB → free
  
  Peak: 11GB (90% reduction!)
```

## Code Flow

```
BuildHashTablesIncrementallyFromShards()
│
├─ Pass 1: Scan metadata
│  └─ For each shard: Load → Extract header → Free
│
└─ Pass 2: Batch processing (loop)
   │
   ├─ Load batch shards (3 files)
   │
   ├─ Determine batch timestep range
   │
   ├─ Extract samples by timestep (parallel)
   │  └─ ParallelFor(shards): Extract samples
   │
   ├─ Free shard data
   │
   ├─ Build hash tables (parallel)
   │  └─ ParallelFor(timesteps):
   │     ├─ BuildHashTableForTimeStep()
   │     └─ SaveToFile()
   │
   └─ Free batch samples
```

## Example

**Dataset:**
- 15 shards
- Each shard: 100 timesteps (e.g., shard-3000.bin has timesteps 3000-3099)
- Batch size: 3 shards

**Processing:**

```
Batch 1: shards 0-2 (timesteps 0-299)
  Load: shard-0.bin, shard-100.bin, shard-200.bin
  Extract: 300 timestep sample arrays
  Build in parallel:
    Thread 1: timestep_000.bin
    Thread 2: timestep_001.bin
    ...
    Thread 300: timestep_299.bin
  Write all 300 hash tables
  Free everything
  
Batch 2: shards 3-5 (timesteps 300-599)
  Load: shard-300.bin, shard-400.bin, shard-500.bin
  Extract: 300 timestep sample arrays
  Build in parallel: timestep_300.bin - timestep_599.bin
  Write all 300 hash tables
  Free everything
  
... continue for all batches ...

Final: 1500 hash table files created
```

## Benefits

### Memory Management

✅ **No accumulation**: Samples not carried across batches
✅ **Constant memory**: Peak memory independent of total shards
✅ **Immediate cleanup**: Everything freed before next batch
✅ **Scalable**: Can process unlimited shards

### Performance

✅ **Parallel extraction**: Shards processed concurrently
✅ **Parallel building**: Timesteps built concurrently
✅ **Independent timesteps**: No synchronization needed between hash tables
✅ **Disk I/O parallelized**: Different files written simultaneously

### Correctness

✅ **Complete hash tables**: Each timestep has all its data from batch
✅ **Final results**: Hash tables never modified after writing
✅ **No dependencies**: Batches are independent

## Comparison

### Previous Approach (Accumulation)

```
Load all shards → Accumulate all samples → Build all hash tables once
Problems:
  - 100GB+ memory
  - OOM errors
  - All-or-nothing processing
```

### Current Approach (Per-Timestep)

```
For each batch: Load → Extract by timestep → Build & write → Free
Benefits:
  - 11GB peak memory (90% reduction)
  - Incremental progress
  - Parallelized at timestep level
```

## Configuration

```cpp
const int32 BatchSize = 3; // Number of shards per batch
```

**Tuning:**
- Smaller: Lower memory, more batches
- Larger: Higher memory, fewer batches
- Recommended: 2-5 shards depending on shard size

## Logging

Example output:

```
[Log] Pass 1 - Scanning 15 shards for time range and bounding box
[Log] Time range: 0 to 1499 (1500 steps)
[Log] Pass 2 - Processing 15 shards in batches of 3

[Log] Processing batch 0-2 (3 shards)
[Log] Batch timestep range: 0 to 299 (300 timesteps)
[Log] Batch 0-2 extracted 15M samples
[Log] Building 300 hash tables in parallel
[Log] Batch 0-2 complete, 300 hash tables built and saved, all data freed

[Log] Processing batch 3-5 (3 shards)
[Log] Batch timestep range: 300 to 599 (300 timesteps)
[Log] Batch 3-5 extracted 15M samples
[Log] Building 300 hash tables in parallel
[Log] Batch 3-5 complete, 300 hash tables built and saved, all data freed

...

[Log] Successfully completed incremental hash table building
```

## Files Modified

- `Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp`:
  - Complete rewrite of Pass 2
  - Per-batch timestep sample extraction
  - Parallel hash table building per timestep
  - Immediate disk writes
  
- `Source/SpatialHashedTrajectory/Public/SpatialHashTableManager.h`:
  - Updated documentation

## Summary

The implementation now correctly:
- Processes shards in batches
- Builds one hash table per timestep
- Writes hash tables immediately
- Frees all data before next batch
- Parallelizes at both shard and timestep levels

This matches the requirement exactly: "load a batch of trajectory data from the shard files and write the corresponding hash table files for these files. The previous loaded batch can now be freed completely."

Memory usage is now constant (11GB) regardless of dataset size, enabling processing of unlimited trajectory data.

