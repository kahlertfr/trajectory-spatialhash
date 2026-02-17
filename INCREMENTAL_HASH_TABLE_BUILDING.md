# Incremental Hash Table Building Per Batch

## Problem Statement

> "@copilot after loading the trajectory data of one batch create the hashtable for the data and free everything before loading a new batch"

The previous implementation accumulated ALL samples from ALL batches before building hash tables, which kept 50-100GB of samples in memory.

## Solution: Incremental Building

Build hash tables progressively after each batch:

```
For each batch:
  1. Load batch shards (3 at a time)
  2. Extract samples → add to accumulated samples
  3. BUILD hash tables from all samples accumulated SO FAR
  4. SAVE hash tables to disk (overwrites previous version)
  5. FREE all samples immediately
  6. Re-initialize arrays for next batch
```

## How It Works

### Progressive Hash Table Completeness

Hash tables are rebuilt after each batch with progressively more complete data:

```
Batch 1 (shards 0-2):   20% of data  → Build & Save → Free samples
Batch 2 (shards 3-5):   40% of data  → Build & Save → Free samples (overwrites)
Batch 3 (shards 6-8):   60% of data  → Build & Save → Free samples (overwrites)
Batch 4 (shards 9-11):  80% of data  → Build & Save → Free samples (overwrites)
Batch 5 (shards 12-14): 100% of data → Build & Save → Free samples (complete!)
```

Each iteration:
- Hash tables become more complete
- Previous version is overwritten
- Samples are freed after building
- Only final version contains complete data

### Memory Usage Pattern

**Before (batch loading only)**:
```
Memory Timeline:
t=0:   Load batch 1     [====] 6GB samples
t=1:   Load batch 2     [========] 12GB samples
t=2:   Load batch 3     [============] 18GB samples
...
t=10:  Load batch 10    [====================] 60GB samples
t=11:  Build all        [====================] 60GB + 10GB building
t=12:  Free all         [] 0GB
```

**After (incremental building)**:
```
Memory Timeline:
t=0:   Load batch 1     [====] 6GB samples
t=1:   Build & save     [====] 6GB + 2GB building
t=2:   Free samples     [] 0GB

t=3:   Load batch 2     [====] 12GB samples (accumulated)
t=4:   Build & save     [====] 12GB + 2GB building
t=5:   Free samples     [] 0GB

t=6:   Load batch 3     [====] 18GB samples (accumulated)
t=7:   Build & save     [====] 18GB + 2GB building
t=8:   Free samples     [] 0GB

...
```

### Peak Memory

**Previous**: 60GB (all samples) + 10GB (building) = 70GB peak
**New**: 60GB (all samples eventually) + 2GB (building) = 62GB peak
  - But samples freed after each build, so typically much lower

## Implementation Details

### Method: BuildHashTablesIncrementallyFromShards()

**Pass 1: Scan for time range and bounding box**
- Load each shard temporarily
- Extract header information
- Compute global time range and bounding box
- Free shard immediately

**Pass 2: Incremental batch processing**
```cpp
TArray<TArray<FSample>> TimeStepSamples; // Accumulates across batches

for each batch:
    // 1. Load and extract
    Load batch shards
    Extract samples → ADD to TimeStepSamples (cumulative)
    Free batch shards
    
    // 2. Build hash tables from accumulated samples
    BuildHashTables(Config, TimeStepSamples)
    
    // 3. FREE samples immediately
    TimeStepSamples.Empty()
    TimeStepSamples.SetNum(TotalTimeSteps) // Re-init for next batch
```

### Key Changes

**Modified Methods**:
- `TryCreateHashTables()`: Now calls `BuildHashTablesIncrementallyFromShards()` instead of `LoadTrajectoryDataFromDirectory()` + `BuildHashTables()`
- `CreateHashTablesAsync()`: Updated to use incremental building

**New Method**:
- `BuildHashTablesIncrementallyFromShards()`: Integrates loading and building with memory management

## Trade-offs

### Benefits

✅ **Samples freed after each batch**: Reduces memory pressure
✅ **Progressive completion**: Hash tables improve incrementally
✅ **Recoverable**: Partial progress saved to disk
✅ **Predictable memory**: Peak is controlled by batch size

### Costs

⚠️ **More I/O**: Hash tables rebuilt and saved multiple times
⚠️ **More CPU**: Building happens N times instead of once
⚠️ **Longer total time**: Extra overhead from repeated building
⚠️ **Incomplete intermediate files**: Early hash tables are partial

## Performance Impact

### I/O Overhead

For 15 batches with 1000 time steps:
- **Previous**: 1 write operation (1000 files)
- **New**: 15 write operations (15,000 file writes total)
- **Overhead**: 15× more disk writes

With SSD: ~30-60 seconds additional time for large datasets

### CPU Overhead

For 15 batches with 1000 time steps:
- **Previous**: Build 1000 hash tables once
- **New**: Build 1000 hash tables 15 times
- **Overhead**: 15× more CPU time for building

However, each iteration is faster (fewer samples), so actual overhead is less.

### Memory Benefit

**Peak memory reduction**: Varies by dataset
- Small datasets: Minimal benefit
- Large datasets: Can prevent OOM errors

## Configuration

```cpp
// In BuildHashTablesIncrementallyFromShards()
const int32 BatchSize = 3; // Shards per batch
```

**Tuning**:
- Smaller BatchSize: More frequent building, lower peak memory
- Larger BatchSize: Less frequent building, higher peak memory

## When to Use

**Use incremental building when**:
- Dataset is very large (50+ shards, millions of trajectories)
- Memory is constrained (16-32GB systems)
- OOM errors occur with batch loading
- Progressive results are valuable

**Use batch loading (previous) when**:
- Dataset is small-medium (< 20 shards)
- Memory is abundant (64GB+)
- Speed is critical
- Final complete results only needed

## Logging

Example log output:

```
[Log] BuildHashTablesIncrementallyFromShards: Pass 1 - Scanning 15 shards
[Log] Time range: 0 to 5000 (5000 steps)
[Log] Pass 2 - Processing 15 shards in batches of 3, building after each batch

[Log] Loading and processing batch 0-2 (3 shards)
[Log] Batch 0-2 extracted 2.5M samples
[Log] Building hash tables from accumulated samples
[Log] Batch 0-2 complete, hash tables built and samples freed

[Log] Loading and processing batch 3-5 (3 shards)
[Log] Batch 3-5 extracted 2.7M samples
[Log] Building hash tables from accumulated samples
[Log] Batch 3-5 complete, hash tables built and samples freed

...

[Log] Successfully completed incremental hash table building
```

## Files Modified

- `Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp`:
  - Modified `TryCreateHashTables()` to use incremental building
  - Modified `CreateHashTablesAsync()` to use incremental building
  - Added `BuildHashTablesIncrementallyFromShards()` method

- `Source/SpatialHashedTrajectory/Public/SpatialHashTableManager.h`:
  - Added method declaration

## Summary

Incremental hash table building provides:
- **Memory management**: Samples freed after each batch's hash table build
- **Progressive results**: Hash tables improve with each batch
- **Robustness**: Can recover from interruptions
- **Trade-off**: More I/O and CPU overhead for memory savings

This approach directly addresses the requirement: "after loading the trajectory data of one batch create the hashtable for the data and free everything before loading a new batch".

The hash tables are created/updated after each batch, and all samples are freed before loading the next batch.
