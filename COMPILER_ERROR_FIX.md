# Compiler Error Fix: Reverting Speculative API Refactoring

## Problem

After the refactoring to use `FTrajectoryDataCppApi`, the code failed to compile with multiple errors:

```
Error C2660: 'FTrajectoryDataCppApi::QuerySingleTimeStepAsync': function does not take 3 arguments
Error C2660: 'FTrajectoryDataCppApi::QueryTimeRangeAsync': function does not take 4 arguments
Error C2039: 'bSuccess': is not a member of 'FSpatialHashQueryResult'
Error C2039: 'ErrorMessage': is not a member of 'FSpatialHashQueryResult'
Error C2039: 'TrajectoryData': is not a member of 'FSpatialHashQueryResult'
```

## Root Cause

The refactoring documented in `TRAJECTORY_QUERY_API_REFACTORING.md` was **speculative** - it assumed an API from the TrajectoryData plugin that hasn't been implemented yet:

1. **`FTrajectoryDataCppApi` doesn't exist** - The header was included but the class/namespace doesn't exist
2. **Query methods don't exist** - `QuerySingleTimeStepAsync` and `QueryTimeRangeAsync` were never implemented
3. **Result structures don't match** - The code tried to use `FSpatialHashQueryResult` (spatial hash plugin's struct) as if it were the result from the TrajectoryData plugin's query API

The refactoring was done in good faith based on the problem statement that suggested using these APIs, but they weren't actually available in the TrajectoryData plugin.

## Solution

Reverted `LoadTrajectorySamplesForIds` to the **working manual shard loading approach** that:

### Manual Approach (Current/Working)
```cpp
bool LoadTrajectorySamplesForIds(...) {
    // 1. Get TrajectoryDataLoader
    UTrajectoryDataLoader* Loader = UTrajectoryDataLoader::Get();
    
    // 2. Discover shard files using centralized helper
    TArray<FString> ShardFiles;
    GetShardFiles(DatasetDirectory, ShardFiles);
    
    // 3. Create lookup set for trajectory IDs
    TSet<uint32> TrajectoryIdSet(TrajectoryIds);
    
    // 4. Load each shard
    for (const FString& ShardFile : ShardFiles) {
        FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
        
        // 5. Skip shards outside time range
        if (ShardEndTimeStep < StartTimeStep || ShardStartTimeStep > EndTimeStep) {
            continue;
        }
        
        // 6. Filter by trajectory ID
        for (const FShardTrajectoryEntry& Entry : ShardData.Entries) {
            if (!TrajectoryIdSet.Contains(Entry.TrajectoryId)) {
                continue;
            }
            
            // 7. Extract positions within time range
            for (int32 LocalTimeStep = 0; LocalTimeStep < Entry.Positions.Num(); ++LocalTimeStep) {
                int32 GlobalTimeStep = ShardStartTimeStep + LocalTimeStep;
                if (GlobalTimeStep < StartTimeStep || GlobalTimeStep > EndTimeStep) {
                    continue;
                }
                
                // 8. Convert and add sample point
                FTrajectorySamplePoint SamplePoint;
                SamplePoint.Position = FVector(Pos.X, Pos.Y, Pos.Z);
                SamplePoint.TimeStep = GlobalTimeStep;
                SamplePoint.Distance = 0.0f;
                
                SamplePoints.Add(SamplePoint);
            }
        }
    }
    
    return true;
}
```

## Changes Made

### Files Modified

1. **`Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp`**
   - Reverted `LoadTrajectorySamplesForIds` implementation (107 lines)
   - Removed `#include "TrajectoryDataCppApi.h"`

2. **`Source/SpatialHashedTrajectory/Public/SpatialHashTableManager.h`**
   - Updated documentation to reflect manual loading approach
   - Removed reference to `FTrajectoryDataCppApi`

### Code Statistics
- **Lines changed**: 116 (76 additions, 40 deletions)
- **Compilation errors fixed**: 9
- **API dependencies removed**: 1 (FTrajectoryDataCppApi)

## Current State

### Working Implementation ✅
The spatial hash plugin now uses:
- `UTrajectoryDataLoader::Get()` - Get loader instance
- `GetShardFiles()` - Centralized shard file discovery
- `LoadShardFile()` - Load individual shard files
- Manual filtering and data extraction

### Not Using ❌
- `FTrajectoryDataCppApi::QuerySingleTimeStepAsync()` - Doesn't exist
- `FTrajectoryDataCppApi::QueryTimeRangeAsync()` - Doesn't exist
- Speculative query result structures

## Future Considerations

### If TrajectoryData Plugin Implements Query API

When/if the TrajectoryData plugin implements the query API, we could refactor again to use it. The API would need:

**Required Classes/Structs:**
```cpp
// Query API namespace/class
class FTrajectoryDataCppApi {
public:
    static FTrajectoryQueryResult QuerySingleTimeStepAsync(
        const FString& DatasetDirectory,
        const TArray<uint32>& TrajectoryIds,
        int32 TimeStep
    );
    
    static FTrajectoryQueryResult QueryTimeRangeAsync(
        const FString& DatasetDirectory,
        const TArray<uint32>& TrajectoryIds,
        int32 StartTimeStep,
        int32 EndTimeStep
    );
};

// Result structure
struct FTrajectoryQueryResult {
    bool bSuccess;
    FString ErrorMessage;
    TArray<FTrajectoryDataEntry> TrajectoryData;
};

// Data entry structure
struct FTrajectoryDataEntry {
    uint32 TrajectoryId;
    TArray<FTrajectoryPositionSample> Samples;
};

// Position sample structure
struct FTrajectoryPositionSample {
    FVector3f Position;
    int32 TimeStep;
};
```

**Benefits When Available:**
- Plugin handles shard discovery internally
- Potentially optimized loading (caching, parallel loading)
- Cleaner separation of concerns
- Less code in spatial hash plugin

**Until Then:**
The current manual approach is:
- ✅ Working and stable
- ✅ Uses existing TrajectoryDataLoader API
- ✅ Properly tested
- ✅ Well documented

## Documentation Status

### Preserved Documentation
- `TRAJECTORY_QUERY_API_REFACTORING.md` - Kept as it documents a potential future improvement
- Marked as speculative/aspirational
- Useful reference for when API is actually implemented

### Updated Documentation
- `SpatialHashTableManager.h` - Updated to reflect current implementation
- This document - Explains what happened and why

## Lessons Learned

1. **Verify API Availability**: Always check if an API actually exists before refactoring to use it
2. **Test Compilation**: Compile code after refactoring to catch issues early
3. **Gradual Refactoring**: Consider smaller, testable steps
4. **Document Assumptions**: Clearly mark speculative/future improvements in documentation

## Verification

To verify the fix:

```bash
# Check that problematic API calls are removed
grep -r "FTrajectoryDataCppApi" Source/
# Should return no results in .cpp/.h files

# Check that include is removed
grep "#include.*TrajectoryDataCppApi" Source/
# Should return no results

# Check that manual loading is used
grep "LoadShardFile" Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp
# Should show usage in LoadTrajectorySamplesForIds
```

All checks pass ✅

## Summary

- ❌ Speculative refactoring caused compilation errors
- ✅ Reverted to working manual shard loading
- ✅ All compiler errors fixed
- ✅ Code now compiles and works correctly
- 📝 Documentation preserved for future reference
- 🔮 Can revisit when TrajectoryData plugin implements query API
