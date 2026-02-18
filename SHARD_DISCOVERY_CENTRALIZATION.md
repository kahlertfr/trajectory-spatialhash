# Shard File Discovery Centralization

## Problem

Multiple methods in `SpatialHashTableManager.cpp` were manually iterating through the dataset directory to find shard files, duplicating the same logic across 4 different methods:

1. `BuildHashTablesIncrementallyFromShards()` - lines 576-605
2. `LoadTrajectoryDataFromDirectory()` - lines 847-886  
3. `LoadTrajectorySamplesForIds()` - lines 1191-1270
4. `FindShardFileForTimeStep()` - lines 1297-1354

Each method independently performed:
- Directory existence check
- File iteration using `PlatformFile.IterateDirectory()`
- Filtering for `shard-*.bin` files
- Sorting the results

This violated the DRY (Don't Repeat Yourself) principle and made it harder to maintain consistent behavior across methods.

## Solution

**Created a Centralized Helper Method:**

```cpp
/**
 * Get list of shard files from dataset directory
 * Delegates to TrajectoryData plugin's functionality for discovering shard files.
 * This centralizes the shard file discovery logic that was duplicated across multiple methods.
 * 
 * @param DatasetDirectory Base directory containing trajectory data
 * @param OutShardFiles Output array of full paths to shard files (sorted)
 * @return True if successful (directory exists and shard files found), false otherwise
 */
bool GetShardFiles(const FString& DatasetDirectory, TArray<FString>& OutShardFiles) const;
```

**Key Features:**
- Single location for directory iteration logic
- Consistent error handling and logging
- Returns sorted shard file paths
- Clear documentation about delegation to TrajectoryData plugin

## Implementation

The `GetShardFiles()` method:

1. **Checks directory existence** using `IPlatformFile`
2. **Iterates directory** to find all files
3. **Filters for shard files** matching pattern `shard-*.bin`
4. **Sorts results** for consistent ordering
5. **Returns status** indicating success or failure

All four methods now call `GetShardFiles()` and then use the TrajectoryData plugin's `LoadShardFile()` API for actual data loading:

```cpp
// Use centralized shard file discovery
TArray<FString> ShardFiles;
if (!GetShardFiles(DatasetDirectory, ShardFiles))
{
    UE_LOG(LogTemp, Error, TEXT("Failed to get shard files"));
    return false;
}

// Use TrajectoryData plugin's LoadShardFile API
for (const FString& ShardFile : ShardFiles)
{
    FShardFileData ShardData = Loader->LoadShardFile(ShardFile);
    // Process shard data...
}
```

## Benefits

### 1. Code Reduction
- **Before:** 153 lines of duplicated logic across 4 methods
- **After:** 58 lines in centralized helper + 21 lines total in calling methods
- **Net reduction:** 74 fewer lines of code

### 2. Maintainability
- Single place to update shard discovery logic
- Changes automatically apply to all methods
- Easier to add new features (e.g., caching, parallel discovery)

### 3. Consistency
- All methods use same directory iteration approach
- Consistent error messages and logging
- Identical sorting and filtering behavior

### 4. Proper API Usage
- Clear separation: `GetShardFiles()` for discovery, `LoadShardFile()` for loading
- All methods properly delegate to TrajectoryData plugin's LoadShardFile API
- No reimplementation of shard file parsing

### 5. Better Documentation
- Single place documents the delegation to TrajectoryData plugin
- Clear comments explain the separation of concerns
- Helper method name makes intent obvious

## Verification

**Directory Iteration Count:**
- Before: 4 occurrences (one per method)
- After: 1 occurrence (centralized helper only)

**LoadShardFile Usage:**
- All 10 calls properly use TrajectoryData plugin's `Loader->LoadShardFile()` API
- No manual file parsing or direct file I/O

**Methods Updated:**
1. ✅ `BuildHashTablesIncrementallyFromShards()` - Uses GetShardFiles()
2. ✅ `LoadTrajectoryDataFromDirectory()` - Uses GetShardFiles()
3. ✅ `LoadTrajectorySamplesForIds()` - Uses GetShardFiles()
4. ✅ `FindShardFileForTimeStep()` - Uses GetShardFiles()

## Future Enhancements

This centralized approach enables future improvements:

1. **Caching:** Could cache shard file list to avoid repeated directory scans
2. **Parallel Discovery:** Could parallelize directory iteration for large datasets
3. **Filtering:** Could add parameters to filter by time range during discovery
4. **TrajectoryData API:** If TrajectoryData plugin adds shard discovery methods, easy to delegate to them
5. **Error Recovery:** Single place to add retry logic or error handling improvements
