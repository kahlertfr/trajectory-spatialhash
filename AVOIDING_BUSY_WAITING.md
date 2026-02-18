# Avoiding Busy-Waiting: Synchronous vs Async Data Loading

## Problem

The previous implementation attempted to use the async TrajectoryDataCppApi (`QuerySingleTimeStepAsync` and `QueryTimeRangeAsync`) but wrapped them in a busy-waiting loop:

```cpp
while (!bQueryComplete) {
    FPlatformProcess::Sleep(0.01f); // ❌ BLOCKS GAME THREAD
}
```

This is unacceptable for game thread performance because:
- Blocks the game thread from processing other work
- Wastes CPU cycles in a tight loop
- Can cause frame rate drops and stuttering
- Goes against Unreal Engine best practices

## Solution

Reverted to **synchronous shard loading** using `UTrajectoryDataLoader::LoadShardFile()`:

```cpp
for (const FString& ShardFile : ShardFiles) {
    FShardFileData ShardData = Loader->LoadShardFile(ShardFile); // ✅ Direct I/O
    // Process data immediately...
}
```

### Why This Works

1. **No Busy-Waiting**: Direct function call, no loops waiting for callbacks
2. **Predictable Performance**: File I/O completes immediately or fails
3. **Simpler Code**: No async state management needed
4. **Appropriate for Use Case**: Query methods are synchronous UFUNCTION calls

## Technical Details

### LoadShardFile() Behavior

The `LoadShardFile()` method from TrajectoryDataLoader:
- Loads a single shard file synchronously
- Returns `FShardFileData` with success status
- Completes in milliseconds for typical shard sizes
- Suitable for on-demand loading when query results are needed immediately

### Query Method Context

The spatial hash query methods are:
- `UFUNCTION(BlueprintCallable)` - synchronous Blueprint calls
- Need immediate results to return to caller
- Can't use latent actions without API changes
- Designed for interactive queries

## When to Use Async API

The TrajectoryDataCppApi async methods are appropriate for:

### ✅ Good Use Cases
- **Latent Blueprint Actions**: Multi-frame operations that don't block
- **Background Preloading**: Load data ahead of time
- **Large Dataset Queries**: When I/O might take significant time
- **Proper Async Workflows**: With state management and callbacks

### ❌ Bad Use Cases (What We Avoided)
- Wrapping in busy-wait loops
- Converting async to sync in game thread
- Immediate-result query methods
- Interactive queries that need instant response

## Performance Considerations

### Synchronous Shard Loading
- **Pros**: 
  - No overhead from async machinery
  - Predictable timing
  - Simple error handling
  - No game thread blocking with sleep
- **Cons**:
  - Large files could stall momentarily
  - Multiple shards load sequentially

### Async with Busy-Wait (Avoided)
- **Pros**: None
- **Cons**:
  - Blocks game thread
  - Wasted CPU cycles
  - Unpredictable frame times
  - Harder to debug

## Future Improvements

If async loading is needed for better performance:

### Option 1: Latent Blueprint Actions
```cpp
UFUNCTION(BlueprintCallable, Category = "Spatial Hash", 
    meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject"))
void QueryRadiusWithDistanceCheckAsync(
    UObject* WorldContextObject,
    FLatentActionInfo LatentInfo,
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float Radius,
    TArray<FSpatialHashQueryResult>& OutResults);
```

Benefits:
- Non-blocking multi-frame operation
- Proper Unreal Engine async pattern
- Can use TrajectoryDataCppApi properly

### Option 2: Preloading/Caching
```cpp
void PreloadTrajectoriesForTimeRange(int32 StartTime, int32 EndTime);
```

Benefits:
- Data ready when queries execute
- No load time during queries
- Better for frequent queries

### Option 3: Task Graph
```cpp
FGraphEventRef QueryTask = FFunctionGraphTask::CreateAndDispatchWhenReady(
    [&]() { /* Load data */ },
    TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);
```

Benefits:
- Truly async without busy-wait
- Uses Unreal's task system
- Can callback on game thread when done

## Code Comparison

### Current Implementation (Good) ✅
```cpp
bool LoadTrajectorySamplesForIds(...) {
    // Get shard files
    TArray<FString> ShardFiles;
    GetShardFiles(DatasetDirectory, ShardFiles);
    
    // Load each shard synchronously
    for (const FString& ShardFile : ShardFiles) {
        FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
        // Process immediately
    }
    
    return true;
}
```
- Direct I/O, no blocking
- Simple and predictable
- Appropriate for synchronous queries

### Previous Implementation (Bad) ❌
```cpp
bool LoadTrajectorySamplesForIds(...) {
    // Start async query
    Api->QueryTimeRangeAsync(..., 
        FOnComplete::CreateLambda([&](const Result&) {
            // Process results
            bComplete = true;
        })
    );
    
    // BUSY-WAIT for callback ❌
    while (!bComplete) {
        FPlatformProcess::Sleep(0.01f); // Blocks game thread!
    }
    
    return true;
}
```
- Blocks game thread
- Wastes CPU cycles
- Poor frame rate
- Not truly async

## Conclusion

The current implementation using synchronous `LoadShardFile()` is the correct approach for:
- Maintaining synchronous API contract
- Avoiding game thread blocking
- Keeping code simple and maintainable
- Providing immediate query results

The async TrajectoryDataCppApi should only be used when implementing proper async workflows like latent Blueprint actions or background preloading tasks.

## Related Files

- `Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp` - Implementation
- `Source/SpatialHashedTrajectory/Public/SpatialHashTableManager.h` - API declarations
- Commit: ee3ee14
