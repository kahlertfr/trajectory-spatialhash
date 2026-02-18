# Refactoring to Use TrajectoryData Plugin Query API

## Problem

The `SpatialHashTableManager` was directly handling trajectory shard file discovery and loading, which violated separation of concerns. Specifically, `LoadTrajectorySamplesForIds()` was:

1. Manually discovering shard files using `GetShardFiles()`
2. Loading each shard file with `LoadShardFile()`
3. Iterating through all entries to filter by trajectory IDs
4. Manually extracting position data and converting formats

This approach had several issues:
- The spatial hash manager shouldn't know about shard files
- Duplicated logic that the TrajectoryData plugin already handles
- Less efficient than letting the plugin optimize the loading
- Made the code harder to maintain

## Solution

Refactored to use the TrajectoryData plugin's `FTrajectoryDataCppApi`, which provides purpose-built query methods:

### QuerySingleTimeStepAsync
```cpp
FTrajectoryQueryResult FTrajectoryDataCppApi::QuerySingleTimeStepAsync(
    const FString& DatasetDirectory,
    const TArray<uint32>& TrajectoryIds,
    int32 TimeStep
);
```
- Queries specific trajectories at a single timestep
- Plugin handles shard discovery and loading internally
- Returns only requested trajectory data

### QueryTimeRangeAsync
```cpp
FTrajectoryQueryResult FTrajectoryDataCppApi::QueryTimeRangeAsync(
    const FString& DatasetDirectory,
    const TArray<uint32>& TrajectoryIds,
    int32 StartTimeStep,
    int32 EndTimeStep
);
```
- Queries specific trajectories over a time range
- Plugin handles overlapping shards and time filtering
- Returns only requested trajectory data within the range

## Implementation

### Before (Manual Approach)
```cpp
bool LoadTrajectorySamplesForIds(...) {
    // Get shard files from directory
    TArray<FString> ShardFiles;
    if (!GetShardFiles(DatasetDirectory, ShardFiles)) {
        return false;
    }
    
    // Create lookup set
    TSet<uint32> TrajectoryIdSet(TrajectoryIds);
    
    // Load each shard
    for (const FString& ShardFile : ShardFiles) {
        FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
        
        // Skip shards outside time range
        if (ShardEndTimeStep < StartTimeStep || ShardStartTimeStep > EndTimeStep) {
            continue;
        }
        
        // Filter by trajectory ID
        for (const FShardTrajectoryEntry& Entry : ShardData.Entries) {
            if (!TrajectoryIdSet.Contains(Entry.TrajectoryId)) {
                continue;
            }
            
            // Extract positions within time range
            for (int32 LocalTimeStep = 0; LocalTimeStep < Entry.Positions.Num(); ++LocalTimeStep) {
                int32 GlobalTimeStep = ShardStartTimeStep + LocalTimeStep;
                if (GlobalTimeStep < StartTimeStep || GlobalTimeStep > EndTimeStep) {
                    continue;
                }
                // Convert and add data...
            }
        }
    }
}
```

### After (Query API Approach)
```cpp
bool LoadTrajectorySamplesForIds(...) {
    if (TrajectoryIds.Num() == 0) {
        return true;
    }
    
    // Determine query type and call appropriate API
    bool bSingleTimeStep = (StartTimeStep == EndTimeStep);
    
    FTrajectoryQueryResult QueryResult;
    if (bSingleTimeStep) {
        QueryResult = FTrajectoryDataCppApi::QuerySingleTimeStepAsync(
            DatasetDirectory, TrajectoryIds, StartTimeStep);
    } else {
        QueryResult = FTrajectoryDataCppApi::QueryTimeRangeAsync(
            DatasetDirectory, TrajectoryIds, StartTimeStep, EndTimeStep);
    }
    
    if (!QueryResult.bSuccess) {
        return false;
    }
    
    // Convert results to our format
    for (const FTrajectoryDataEntry& Entry : QueryResult.TrajectoryData) {
        for (const FTrajectoryPositionSample& Sample : Entry.Samples) {
            // Add sample point...
        }
    }
    
    return true;
}
```

## Benefits

### 1. Separation of Concerns
- SpatialHashTableManager focuses on spatial hashing
- TrajectoryData plugin handles trajectory data loading
- Clear responsibility boundaries

### 2. Code Reduction
- **Before:** ~105 lines with manual shard handling
- **After:** ~70 lines using query API
- **Reduction:** 35 lines (33% less code)

### 3. Better Optimization
- Plugin can optimize shard loading (caching, parallel loading, etc.)
- Plugin knows which shards contain which trajectories
- Can avoid loading unnecessary data

### 4. Easier Maintenance
- Changes to shard format only affect TrajectoryData plugin
- No need to update spatial hash code when data format changes
- Cleaner, more readable code

### 5. API Consistency
- Other parts of the system can use same query methods
- Consistent error handling through QueryResult
- Standard way to query trajectory data

## What Didn't Change

### Methods Still Using Manual Shard Loading
These methods need access to **all** trajectory data for building hash tables, not filtered subsets:

1. **`BuildHashTablesIncrementallyFromShards()`**
   - Needs complete dataset to build spatial index
   - Must process every trajectory
   - Uses `GetShardFiles()` appropriately

2. **`LoadTrajectoryDataFromDirectory()`**
   - Loads all trajectory data for hash table construction
   - Requires complete time range coverage
   - Uses `GetShardFiles()` appropriately

### Helper Methods Retained

1. **`GetShardFiles()`**
   - Still used by hash table building methods
   - Centralizes shard discovery for those use cases
   - May eventually be replaced if plugin adds "build from dataset" APIs

2. **`FindShardFileForTimeStep()`**
   - Currently unused but kept for API compatibility
   - Could be removed in future cleanup

## Data Structure Mapping

The refactoring requires mapping between plugin types and internal types:

### TrajectoryData Plugin Types
- `FTrajectoryQueryResult` - Query result container
- `FTrajectoryDataEntry` - Single trajectory's data
- `FTrajectoryPositionSample` - Position at specific timestep

### Internal Types
- `FTrajectorySamplePoint` - Our sample point format
- Position converted from `FVector3f` to `FVector`
- Distance field initialized to 0.0f (calculated later if needed)

## Future Enhancements

With this refactoring in place, future improvements become easier:

1. **Async Loading**
   - Methods are already named "Async"
   - Could add true async support without changing callers

2. **Caching**
   - Plugin can cache frequently queried trajectories
   - Transparent to spatial hash manager

3. **Incremental Loading**
   - Plugin could stream data for very large queries
   - No changes needed to spatial hash code

4. **Query Optimization**
   - Plugin can optimize based on query patterns
   - Can pre-index frequently queried trajectories

## Migration Notes

### For Users
No API changes for users of `SpatialHashTableManager`:
- Same method signatures
- Same behavior
- Same results

### For Plugin Developers
If updating TrajectoryData plugin:
- Ensure `FTrajectoryDataCppApi` is available
- Implement `QuerySingleTimeStepAsync`
- Implement `QueryTimeRangeAsync`
- Provide result structures: `FTrajectoryQueryResult`, `FTrajectoryDataEntry`, `FTrajectoryPositionSample`

### For Maintainers
- Keep `GetShardFiles()` until hash table building can be refactored
- Consider removing `FindShardFileForTimeStep()` if confirmed unused
- Monitor for other methods that could use query API
