# Fixed Radius Nearest Neighbor Query Guide

This guide explains how to use the new fixed radius nearest neighbor query functionality with actual distance calculations using the TrajectoryData plugin.

## Overview

The spatial hash table now supports advanced nearest neighbor queries that:
- Gather candidate trajectory IDs from the spatial hash structure
- Load actual trajectory sample data from the TrajectoryData plugin
- Compute precise distances between query points and trajectory samples
- Support multiple query configurations (single/multiple points, time ranges, dual radius)

## Query Types

### Case A: Single Point, Single Timestep Query

Returns one sample point per trajectory that is within the query radius at a specific timestep.

**C++ Example:**
```cpp
#include "SpatialHashTableManager.h"

// Assume manager is already created and hash tables are loaded
USpatialHashTableManager* Manager = ...;
FString DatasetDirectory = TEXT("/Path/To/Dataset");

// Query parameters
FVector QueryPosition(100.0f, 200.0f, 50.0f);
float Radius = 50.0f;
float CellSize = 10.0f;
int32 TimeStep = 100;

// Execute query
TArray<FSpatialHashQueryResult> Results;
int32 NumFound = Manager->QueryRadiusWithDistanceCheck(
    DatasetDirectory,
    QueryPosition,
    Radius,
    CellSize,
    TimeStep,
    Results);

// Process results
for (const FSpatialHashQueryResult& Result : Results)
{
    UE_LOG(LogTemp, Log, TEXT("Trajectory %d has %d samples within radius"),
        Result.TrajectoryId, Result.SamplePoints.Num());
    
    for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
    {
        UE_LOG(LogTemp, Log, TEXT("  Position: (%.2f, %.2f, %.2f), Distance: %.2f, TimeStep: %d"),
            Sample.Position.X, Sample.Position.Y, Sample.Position.Z,
            Sample.Distance, Sample.TimeStep);
    }
}
```

**Blueprint Usage:**
1. Get reference to Spatial Hash Table Manager
2. Call "Query Radius With Distance Check" node
3. Provide:
   - Dataset Directory (path to trajectory data)
   - Query Position
   - Radius
   - Cell Size (must match loaded hash tables)
   - Time Step
4. Receive array of Trajectory Query Results
5. Loop through results to process each trajectory and its sample points

### Case B: Single Point, Time Range Query

Returns trajectories that have at least one sample within the query radius during the specified time range.

**C++ Example:**
```cpp
FVector QueryPosition(100.0f, 200.0f, 50.0f);
float Radius = 50.0f;
float CellSize = 10.0f;
int32 StartTimeStep = 100;
int32 EndTimeStep = 200;

TArray<FSpatialHashQueryResult> Results;
int32 NumFound = Manager->QueryRadiusOverTimeRange(
    DatasetDirectory,
    QueryPosition,
    Radius,
    CellSize,
    StartTimeStep,
    EndTimeStep,
    Results);

// Results contain all sample points in the time range for trajectories
// that have at least one sample within the radius
```

### Case C: Query Trajectory Over Time Range

Returns trajectories that intersect with a query trajectory's radius during the time range. For each matching trajectory, includes all sample points from when it first enters until it last exits the query radius (accounting for re-entry).

**C++ Example:**
```cpp
int32 QueryTrajectoryId = 42;
float Radius = 50.0f;
float CellSize = 10.0f;
int32 StartTimeStep = 100;
int32 EndTimeStep = 200;

TArray<FSpatialHashQueryResult> Results;
int32 NumFound = Manager->QueryTrajectoryRadiusOverTimeRange(
    DatasetDirectory,
    QueryTrajectoryId,
    Radius,
    CellSize,
    StartTimeStep,
    EndTimeStep,
    Results);

// Results contain extended sample ranges from first entry to last exit
// for each trajectory that intersects with the query trajectory
```

### Dual Radius Query

Query with both inner and outer radius simultaneously. Returns two separate result arrays:
- Inner radius results: trajectories within the inner radius
- Outer radius results: trajectories within the outer radius (includes inner radius samples for consistent trajectories)

**C++ Example:**
```cpp
FVector QueryPosition(100.0f, 200.0f, 50.0f);
float InnerRadius = 25.0f;
float OuterRadius = 50.0f;
float CellSize = 10.0f;
int32 TimeStep = 100;

TArray<FSpatialHashQueryResult> InnerResults;
TArray<FSpatialHashQueryResult> OuterResults;

int32 TotalFound = Manager->QueryDualRadiusWithDistanceCheck(
    DatasetDirectory,
    QueryPosition,
    InnerRadius,
    OuterRadius,
    CellSize,
    TimeStep,
    InnerResults,
    OuterResults);

UE_LOG(LogTemp, Log, TEXT("Found %d in inner radius, %d in outer radius"),
    InnerResults.Num(), OuterResults.Num());
```

## Data Structures

### FTrajectorySamplePoint

Represents a single sample point from a trajectory:

```cpp
struct FTrajectorySamplePoint
{
    FVector Position;      // World position of the sample
    int32 TimeStep;        // Time step of this sample
    float Distance;        // Distance from query point
};
```

### FSpatialHashQueryResult

Contains all sample points for a trajectory that match the query:

```cpp
struct FSpatialHashQueryResult
{
    int32 TrajectoryId;                          // Trajectory ID
    TArray<FTrajectorySamplePoint> SamplePoints; // All matching sample points
};
```

## Implementation Details

### Two-Phase Query Process

1. **Spatial Hash Query**: Quickly gather candidate trajectory IDs from cells that overlap with the query radius
2. **Distance Verification**: Load actual trajectory data and compute precise distances

### TrajectoryData Integration

The implementation uses the TrajectoryData plugin's `LoadShardFile` API to:
- Find which shard files contain the requested time steps
- Load trajectory data for specific trajectory IDs
- Extract position information for distance calculations

### Memory Optimization

- Only loads trajectory data for candidates from spatial hash
- Processes shards efficiently to minimize memory usage
- Supports large time ranges by streaming data as needed

## Performance Considerations

### Cell Size Selection

- Smaller cell sizes: More precise spatial hash, but more cells to check
- Larger cell sizes: Fewer cells to check, but more candidate trajectories
- Recommended: Cell size approximately equal to expected query radius

### Time Range Queries

- Loading many timesteps requires loading multiple shard files
- Consider loading hash tables for the time range beforehand
- For very large time ranges, consider processing in batches

### Dual Radius Queries

- More efficient than two separate queries
- Only loads trajectory data once for the outer radius
- Filters into two result arrays during distance calculation
- Outer radius results include inner radius samples for consistent trajectories without additional lookups

## Error Handling

All query methods include error handling for:
- Missing hash tables (returns 0, logs warning)
- Invalid time ranges (returns 0, logs error)
- Failed trajectory data loading (returns 0, logs error)
- Invalid dual radius configuration (inner > outer, returns 0, logs error)

## Prerequisites

1. **Hash Tables Must Be Loaded**:
   ```cpp
   Manager->LoadHashTables(DatasetDirectory, CellSize, StartTimeStep, EndTimeStep);
   ```

2. **TrajectoryData Plugin**:
   - Must be installed and enabled
   - Dataset directory must contain shard-*.bin files
   - See [TrajectoryData documentation](https://github.com/kahlertfr/ue-plugin-trajectory-data)

3. **Dataset Structure**:
   ```
   DatasetDirectory/
   ├── shard-0000.bin
   ├── shard-0100.bin
   ├── shard-0200.bin
   └── ...
   ```

## Example: Complete Workflow

```cpp
// 1. Create and initialize manager
USpatialHashTableManager* Manager = NewObject<USpatialHashTableManager>();

FString DatasetDirectory = FPaths::ProjectContentDir() + TEXT("Data/Trajectories");
float CellSize = 10.0f;
int32 StartTimeStep = 0;
int32 EndTimeStep = 1000;

// 2. Load hash tables (auto-create if needed)
int32 LoadedCount = Manager->LoadHashTables(
    DatasetDirectory, CellSize, StartTimeStep, EndTimeStep, true);

if (LoadedCount == 0)
{
    UE_LOG(LogTemp, Error, TEXT("Failed to load hash tables"));
    return;
}

// 3. Perform query with distance checking
FVector QueryPosition = GetActorLocation();
float Radius = 50.0f;
int32 QueryTimeStep = 500;

TArray<FSpatialHashQueryResult> Results;
int32 NumFound = Manager->QueryRadiusWithDistanceCheck(
    DatasetDirectory,
    QueryPosition,
    Radius,
    CellSize,
    QueryTimeStep,
    Results);

// 4. Visualize or process results
for (const FSpatialHashQueryResult& Result : Results)
{
    for (const FTrajectorySamplePoint& Sample : Result.SamplePoints)
    {
        // Draw debug sphere at sample location
        DrawDebugSphere(
            GetWorld(),
            Sample.Position,
            5.0f,
            12,
            FColor::Green,
            false,
            5.0f);
    }
}
```

## Limitations

1. **Case D Not Implemented**: Single timestep with multiple query points can be simulated by calling Case A multiple times
2. **Memory for Large Queries**: Very large time ranges or many candidate trajectories may require significant memory
3. **Shard Loading**: Each query loads shard data fresh; consider caching if performing many queries

## Future Enhancements

Potential improvements for future versions:
- Trajectory data caching for repeated queries
- Incremental result streaming for very large queries
- GPU-accelerated distance calculations
- Advanced filtering (velocity, acceleration, etc.)
