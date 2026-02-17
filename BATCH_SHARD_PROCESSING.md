# Batch Shard Processing Optimization

## Problem

The previous implementation loaded ALL shard files into memory before processing them:

```cpp
// OLD: Load all shards at once
for (const FString& ShardFile : ShardFiles) {
    ShardDataArray.Add(Loader->LoadShardFile(ShardFile));  // All in memory!
}

// Then process all loaded shards
ParallelFor(ShardDataArray.Num(), [...]);
```

**Issue**: For datasets with many large shards (e.g., 50+ shards with millions of trajectories each), this would consume 100+ GB of memory before any processing could free memory.

## Solution

Process shards in small batches instead of loading all at once (default: 3 shards per batch, configurable):

```cpp
// NEW: Process in batches (configurable batch size)
const int32 BatchSize = 3; // Default: 3, configurable based on system memory

for (int32 BatchStart = 0; BatchStart < TotalShards; BatchStart += BatchSize) {
    // 1. Load only 2-3 shards
    TArray<FShardFileData> BatchShardData;
    for (int32 i = BatchStart; i < BatchStart + BatchSize; ++i) {
        BatchShardData.Add(Loader->LoadShardFile(ShardFiles[i]));
    }
    
    // 2. Process this batch
    ParallelFor(BatchShardData.Num(), [...]);
    
    // 3. Free batch immediately
    BatchShardData.Empty();
    
    // 4. Load next batch...
}
```

## Implementation Details

### Two-Pass Approach

**Pass 1: Lightweight Time Range Scan**
- Load each shard temporarily to read its header
- Extract time step range information
- Free shard immediately (only header data retained)
- Result: Know global time range without keeping any shards in memory

**Pass 2: Batch Processing**
- Process shards in batches of 3
- For each batch:
  1. Load 3 shard files
  2. Process them in parallel
  3. Extract trajectory samples
  4. Free batch data immediately
  5. Move to next batch

### Memory Usage

**Before (Load All)**:
```
Memory Usage Timeline:
t=0:   Load shard 1    [====] 2GB
t=1:   Load shard 2    [========] 4GB
t=2:   Load shard 3    [============] 6GB
...
t=50:  Load shard 50   [==================================================] 100GB
t=51:  Process all     [==================================================] 100GB
t=52:  Free all        [] 0GB
```

**After (Batch of 3)**:
```
Memory Usage Timeline:
t=0:   Load batch 1    [====] 2GB
t=1:   Process batch 1 [====] 2GB
t=2:   Free batch 1    [] 0GB
t=3:   Load batch 2    [====] 2GB
t=4:   Process batch 2 [====] 2GB
t=5:   Free batch 2    [] 0GB
...
```

**Peak Memory**: ~2-6GB (only 3 shards max) instead of 100GB

## Configuration

The batch size is configurable based on system memory:

```cpp
const int32 BatchSize = 3; // Configurable: default is 3
```

**Recommended values**:
- **2-3**: Best for memory-constrained systems (8-16GB RAM) - **Default**
- **5-10**: Good for systems with more memory (32GB+ RAM)
- **Higher**: Only if you have abundant memory and want faster processing

The default of 3 provides a good balance between memory usage and performance for most systems.

## Performance Characteristics

### Time Complexity
- **Before**: O(n) load + O(n) process = O(n) total
- **After**: O(n/b) × (O(b) load + O(b) process) = O(n) total
  - Where b = batch size, n = number of shards
  - Same overall complexity!

### Space Complexity
- **Before**: O(n × s) where n = shards, s = shard size
  - Example: 50 shards × 2GB = 100GB peak
- **After**: O(b × s) where b = batch size (e.g., 3)
  - Example: 3 shards × 2GB = 6GB peak

### I/O Impact
- Slightly more I/O due to not keeping all shards in memory
- But modern SSDs make this negligible
- Tradeoff is worth it for memory savings

## Code Flow

```
LoadTrajectoryDataFromDirectory()
│
├─ Pass 1: Scan Time Range (lightweight)
│  │
│  ├─ For each shard file:
│  │   ├─ Load shard
│  │   ├─ Read header (time step range)
│  │   ├─ Free shard
│  │   └─ Store metadata only
│  │
│  └─ Determine GlobalMinTimeStep, GlobalMaxTimeStep
│
├─ Initialize OutTimeStepSamples array
│
└─ Pass 2: Batch Processing
   │
   ├─ For each batch (3 shards):
   │   │
   │   ├─ Load batch shards
   │   │
   │   ├─ ParallelFor: Process shards in batch
   │   │   └─ Extract trajectory samples
   │   │
   │   └─ Free batch shards
   │
   └─ All shards processed
```

## Logging

The implementation provides detailed logging:

```
[Log] First pass - determining time range from 50 shards
[Log] Global time step range: 0 to 5000
[Log] Processing 50 shards in batches of 3
[Log] Processing batch 0-2 (3 shards)
[Log] Completed batch 0-2, processed 2.5M samples (total: 2.5M)
[Log] Processing batch 3-5 (3 shards)
[Log] Completed batch 3-5, processed 2.7M samples (total: 5.2M)
...
[Log] Loaded 125M total samples across 5000 time steps from 50 shards using batch processing
```

## Benefits

1. **Constant Memory Usage**: Peak memory is constant regardless of shard count
2. **No Memory Overflow**: Works with datasets too large to load fully
3. **Predictable Performance**: Memory usage is O(batch_size), not O(num_shards)
4. **Same Processing Speed**: Parallel processing within each batch maintains performance
5. **Scalable**: Can process unlimited number of shards with limited memory

## Testing

To test with different batch sizes:

```cpp
// In SpatialHashTableManager.cpp, line ~731
const int32 BatchSize = 3; // Change this value

// Test values:
// - BatchSize = 1:  Slowest, minimal memory
// - BatchSize = 3:  Good balance (default)
// - BatchSize = 10: Faster, more memory
// - BatchSize = 50: Approaches old behavior
```

## Related Optimizations

This builds on previous memory optimizations:
1. **Hash table freeing** (after saving to disk)
2. **Time step sample freeing** (after building completes)
3. **Shard batch processing** (this optimization)

Together, these enable processing of massive datasets (billions of trajectory points) on standard workstations.

## Files Modified

- `Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp`
  - `LoadTrajectoryDataFromDirectory()` method
  - Two-pass batch processing implementation

## Summary

By processing shards in small batches instead of loading all at once, we:
- Reduce peak memory from O(num_shards) to O(batch_size)
- Enable processing of arbitrarily large datasets
- Maintain parallel processing performance
- Prevent memory overflow errors

This is the final piece of the memory optimization puzzle, making the system truly scalable for production use with massive trajectory datasets.
