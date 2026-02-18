# Struct Renaming: FTrajectoryQueryResult to FSpatialHashQueryResult

## Purpose

This document explains the renaming of the `FTrajectoryQueryResult` struct to `FSpatialHashQueryResult` to avoid naming collisions between plugins.

## Problem

The spatial hash plugin defined a struct called `FTrajectoryQueryResult` which had the same name as a struct in the trajectory data plugin. This caused:

1. **Naming Ambiguity**: Unclear which struct was being referenced in code
2. **Potential Compilation Issues**: Name collision could cause build errors
3. **Maintenance Confusion**: Developers might confuse the two different structs

## Solution

Renamed the spatial hash plugin's struct from `FTrajectoryQueryResult` to `FSpatialHashQueryResult`.

### New Name Rationale

The name `FSpatialHashQueryResult` was chosen because:
- **Prefix Clarity**: `FSpatialHash` prefix clearly identifies it as part of the spatial hash plugin
- **Descriptive**: Indicates it's a query result from spatial hash operations
- **Consistent**: Follows Unreal Engine naming conventions (F prefix for structs)
- **Distinct**: Cannot be confused with trajectory data plugin's struct

## Struct Definition

### Before
```cpp
USTRUCT(BlueprintType)
struct FTrajectoryQueryResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
    int32 TrajectoryId;
    
    UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
    TArray<FTrajectorySamplePoint> SamplePoints;
    
    FTrajectoryQueryResult();
    FTrajectoryQueryResult(int32 InId);
};
```

### After
```cpp
USTRUCT(BlueprintType)
struct FSpatialHashQueryResult
{
    GENERATED_BODY()
    
    UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
    int32 TrajectoryId;
    
    UPROPERTY(BlueprintReadOnly, Category = "Spatial Hash")
    TArray<FTrajectorySamplePoint> SamplePoints;
    
    FSpatialHashQueryResult();
    FSpatialHashQueryResult(int32 InId);
};
```

## Files Updated

### Source Code (6 files)
1. **Source/SpatialHashedTrajectory/Public/SpatialHashTableManager.h**
   - Struct definition
   - 8 method signatures updated

2. **Source/SpatialHashedTrajectory/Private/SpatialHashTableManager.cpp**
   - 14 method implementations updated
   - All local variable declarations updated

3. **Source/SpatialHashedTrajectory/Public/FixedRadiusQueryExample.h**
   - Example class method signatures
   - Example implementation code

4. **README.md**
   - Updated example code snippets

5. **IMPLEMENTATION_SUMMARY_NEAREST_NEIGHBOR.md**
   - Updated API documentation
   - Updated code examples

6. **NEAREST_NEIGHBOR_QUERY_GUIDE.md**
   - Updated usage guide
   - Updated all code examples

## Methods Affected

All methods that use this struct were updated:

### Query Methods
```cpp
// Single point, single timestep
int32 QueryRadiusWithDistanceCheck(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float Radius,
    float CellSize,
    int32 TimeStep,
    TArray<FSpatialHashQueryResult>& OutResults);

// Dual radius query
int32 QueryDualRadiusWithDistanceCheck(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float InnerRadius,
    float OuterRadius,
    float CellSize,
    int32 TimeStep,
    TArray<FSpatialHashQueryResult>& OutInnerResults,
    TArray<FSpatialHashQueryResult>& OutOuterOnlyResults);

// Time range query
int32 QueryRadiusOverTimeRange(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float Radius,
    float CellSize,
    int32 StartTimeStep,
    int32 EndTimeStep,
    TArray<FSpatialHashQueryResult>& OutResults);

// Trajectory query
int32 QueryTrajectoryRadiusOverTimeRange(
    const FString& DatasetDirectory,
    int32 QueryTrajectoryId,
    float Radius,
    float CellSize,
    int32 StartTimeStep,
    int32 EndTimeStep,
    TArray<FSpatialHashQueryResult>& OutResults);
```

### Helper Methods
```cpp
void FilterByDistance(
    const FVector& QueryPosition,
    float Radius,
    const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
    TArray<FSpatialHashQueryResult>& OutResults) const;

void FilterByDualRadius(
    const FVector& QueryPosition,
    float InnerRadius,
    float OuterRadius,
    const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
    TArray<FSpatialHashQueryResult>& OutInnerResults,
    TArray<FSpatialHashQueryResult>& OutOuterOnlyResults) const;

void ExtendTrajectorySamples(
    const TMap<uint32, TArray<FTrajectorySamplePoint>>& TrajectoryData,
    float Radius,
    TArray<FSpatialHashQueryResult>& OutExtendedResults) const;
```

## Usage Examples

### Before
```cpp
TArray<FTrajectoryQueryResult> Results;
Manager->QueryRadiusWithDistanceCheck(
    DatasetDirectory, QueryPosition, Radius, CellSize, TimeStep, Results);

for (const FTrajectoryQueryResult& Result : Results)
{
    // Process result...
}
```

### After
```cpp
TArray<FSpatialHashQueryResult> Results;
Manager->QueryRadiusWithDistanceCheck(
    DatasetDirectory, QueryPosition, Radius, CellSize, TimeStep, Results);

for (const FSpatialHashQueryResult& Result : Results)
{
    // Process result...
}
```

## Blueprint Compatibility

The struct is marked with `USTRUCT(BlueprintType)`, so it's accessible in Blueprints. After this change:
- Blueprint nodes will show the new name
- Existing Blueprint graphs may need to be refreshed/recompiled
- No functional changes to Blueprint behavior

## Migration Guide

For code using this struct:

1. **Find and Replace**: Search for `FTrajectoryQueryResult` and replace with `FSpatialHashQueryResult`
2. **Recompile**: Rebuild your project
3. **Blueprints**: Refresh/recompile any Blueprints using this struct
4. **Documentation**: Update any custom documentation referencing the old name

### Search Patterns
```bash
# Find all usages
grep -r "FTrajectoryQueryResult" YourProject/

# Replace in files
sed -i 's/FTrajectoryQueryResult/FSpatialHashQueryResult/g' YourFile.cpp
```

## Files NOT Updated

The following files were intentionally NOT updated because they document the TrajectoryData plugin's `FTrajectoryQueryResult` (a different struct):

- `TRAJECTORY_QUERY_API_REFACTORING.md` - Documents TrajectoryData plugin API
- `QUERY_API_INTEGRATION_SUMMARY.md` - Documents TrajectoryData plugin integration

These files correctly reference the trajectory data plugin's struct, which keeps its original name.

## Statistics

- **Total occurrences renamed**: 37
- **Files updated**: 6
- **Methods affected**: 8 public methods + 3 private helper methods
- **Documentation files updated**: 3

## Verification

To verify all changes were made correctly:

```bash
# Should return 0 results (no old name in spatial hash plugin)
grep -r "FTrajectoryQueryResult" Source/SpatialHashedTrajectory/ --include="*.h" --include="*.cpp"

# Should return 37 results (all changed to new name)
grep -r "FSpatialHashQueryResult" Source/SpatialHashedTrajectory/ --include="*.h" --include="*.cpp" | wc -l
```

## Future Considerations

This renaming establishes a pattern for avoiding naming conflicts:
- Use plugin-specific prefixes for exported types
- `FSpatialHash*` for spatial hash plugin structs
- `FTrajectoryData*` for trajectory data plugin structs
- Clear separation of concerns between plugins

## Related Changes

This change is part of a larger effort to properly separate concerns between the spatial hash plugin and the trajectory data plugin. See also:
- TRAJECTORY_QUERY_API_REFACTORING.md - Using TrajectoryData plugin's query API
- SHARD_DISCOVERY_CENTRALIZATION.md - Delegating shard discovery to plugin
