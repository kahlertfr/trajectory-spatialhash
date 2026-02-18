# Async Query Methods Documentation

## Overview

This document explains the asynchronous query methods added to `USpatialHashTableManager`. These methods use the TrajectoryData plugin's `FTrajectoryDataCppApi` for non-blocking trajectory data loading with proper callback handling.

## Key Concepts

### Why Async?

**Synchronous Methods:**
- Block game thread during file I/O
- Can cause frame rate stutters
- Acceptable for small datasets only
- Return results immediately

**Async Methods (New):**
- Non-blocking - game thread continues
- File I/O happens on background thread
- Smooth frame rate for any dataset size
- Results delivered via callback

### No Busy-Waiting

Unlike the problematic approach of wrapping async calls in `while (!bComplete)` loops, these methods use **proper callbacks**:

```cpp
// ❌ BAD: Busy-waiting (blocks game thread)
while (!bQueryComplete) {
    FPlatformProcess::Sleep(0.01f);
}

// ✅ GOOD: Callback (non-blocking)
Api->QueryAsync(..., 
    FOnComplete::CreateLambda([](const Result&) {
        // Process results here
    })
);
// Code continues immediately
```

## Available Async Methods

### 1. QueryRadiusWithDistanceCheckAsync

Query trajectories within a radius at a single timestep.

**Signature:**
```cpp
void QueryRadiusWithDistanceCheckAsync(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float Radius,
    float CellSize,
    int32 TimeStep,
    FOnSpatialHashQueryComplete OnComplete);
```

**Usage Example:**
```cpp
USpatialHashTableManager* Manager = GetSpatialHashManager();

Manager->QueryRadiusWithDistanceCheckAsync(
    TEXT("C:/Data/Trajectories"),
    FVector(100, 200, 50),  // Query position
    500.0f,                  // Radius
    10.0f,                   // Cell size
    100,                     // Time step
    FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results) {
        UE_LOG(LogTemp, Log, TEXT("Query complete! Found %d trajectories"), Results.Num());
        
        for (const FSpatialHashQueryResult& Result : Results) {
            UE_LOG(LogTemp, Log, TEXT("  Trajectory %d with %d samples"), 
                Result.TrajectoryId, Result.SamplePoints.Num());
        }
    })
);

UE_LOG(LogTemp, Log, TEXT("Query started - game thread continues..."));
```

### 2. QueryDualRadiusWithDistanceCheckAsync

Query trajectories with inner and outer radius simultaneously.

**Signature:**
```cpp
void QueryDualRadiusWithDistanceCheckAsync(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float InnerRadius,
    float OuterRadius,
    float CellSize,
    int32 TimeStep,
    FOnSpatialHashDualQueryComplete OnComplete);
```

**Usage Example:**
```cpp
Manager->QueryDualRadiusWithDistanceCheckAsync(
    DatasetDir, QueryPos,
    100.0f,  // Inner radius
    500.0f,  // Outer radius
    10.0f, 50,
    FOnSpatialHashDualQueryComplete::CreateLambda([](
        const TArray<FSpatialHashQueryResult>& InnerResults,
        const TArray<FSpatialHashQueryResult>& OuterOnlyResults) {
        
        UE_LOG(LogTemp, Log, TEXT("Inner: %d, Outer only: %d"), 
            InnerResults.Num(), OuterOnlyResults.Num());
    })
);
```

### 3. QueryRadiusOverTimeRangeAsync

Query trajectories within a radius over a time range.

**Signature:**
```cpp
void QueryRadiusOverTimeRangeAsync(
    const FString& DatasetDirectory,
    FVector QueryPosition,
    float Radius,
    float CellSize,
    int32 StartTimeStep,
    int32 EndTimeStep,
    FOnSpatialHashQueryComplete OnComplete);
```

**Usage Example:**
```cpp
Manager->QueryRadiusOverTimeRangeAsync(
    DatasetDir, QueryPos, 500.0f, 10.0f,
    0,     // Start time
    100,   // End time
    FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results) {
        for (const FSpatialHashQueryResult& Result : Results) {
            UE_LOG(LogTemp, Log, TEXT("Trajectory %d: %d samples over time"), 
                Result.TrajectoryId, Result.SamplePoints.Num());
            
            // Access individual samples
            for (const FTrajectorySamplePoint& Sample : Result.SamplePoints) {
                UE_LOG(LogTemp, Log, TEXT("  t=%d: pos=(%.1f, %.1f, %.1f) dist=%.1f"),
                    Sample.TimeStep, 
                    Sample.Position.X, Sample.Position.Y, Sample.Position.Z,
                    Sample.Distance);
            }
        }
    })
);
```

### 4. QueryTrajectoryRadiusOverTimeRangeAsync

Query trajectories near a query trajectory over a time range.

**Signature:**
```cpp
void QueryTrajectoryRadiusOverTimeRangeAsync(
    const FString& DatasetDirectory,
    uint32 QueryTrajectoryId,
    float Radius,
    float CellSize,
    int32 StartTimeStep,
    int32 EndTimeStep,
    FOnSpatialHashQueryComplete OnComplete);
```

**Usage Example:**
```cpp
Manager->QueryTrajectoryRadiusOverTimeRangeAsync(
    DatasetDir,
    12345,    // Query trajectory ID
    200.0f,   // Radius around query trajectory
    10.0f,
    0, 100,
    FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results) {
        UE_LOG(LogTemp, Log, TEXT("Found %d trajectories near query trajectory"), Results.Num());
    })
);
```

## Callback Delegates

### FOnSpatialHashQueryComplete

Callback for single result array.

```cpp
DECLARE_DELEGATE_OneParam(FOnSpatialHashQueryComplete, const TArray<FSpatialHashQueryResult>&);
```

**Creating Callbacks:**
```cpp
// Lambda (most common)
FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results) {
    // Use results
});

// UObject method
FOnSpatialHashQueryComplete::CreateUObject(this, &UMyClass::OnQueryComplete);

// Raw function pointer
FOnSpatialHashQueryComplete::CreateRaw(this, &MyClass::OnQueryComplete);

// Static function
FOnSpatialHashQueryComplete::CreateStatic(&MyClass::OnQueryCompleteStatic);
```

### FOnSpatialHashDualQueryComplete

Callback for dual result arrays (inner/outer radius).

```cpp
DECLARE_DELEGATE_TwoParams(FOnSpatialHashDualQueryComplete, 
    const TArray<FSpatialHashQueryResult>&, 
    const TArray<FSpatialHashQueryResult>&);
```

## Implementation Details

### Data Flow

1. **Spatial Hash Query** (fast, synchronous)
   - Get candidate trajectory IDs from spatial hash
   - No file I/O, just hash table lookup

2. **Async Data Load** (non-blocking)
   - Convert IDs to int64 for TrajectoryDataCppApi
   - Start async query with callback
   - Method returns immediately

3. **Background Processing**
   - TrajectoryDataCppApi loads shards on background thread
   - Filters by trajectory IDs
   - Extracts position data

4. **Callback Invocation** (game thread)
   - TrajectoryDataCppApi invokes callback on game thread
   - Lambda processes results
   - Converts data to internal format
   - Filters by distance
   - Invokes user callback

### Type Conversions

The async flow handles type conversions transparently:

```cpp
// Internal → API
uint32 TrajectoryId → int64 TrajectoryId

// API → Internal
FTrajectorySample → FTrajectorySamplePoint
FTrajectoryTimeSeries → TArray<FTrajectorySamplePoint>
int64 TrajectoryId → uint32 TrajectoryId
```

### Error Handling

All async methods handle errors gracefully:

```cpp
// API failure
if (!Api) {
    OnComplete.ExecuteIfBound(TArray<FSpatialHashQueryResult>());
    return;
}

// Query start failure
if (!bStarted) {
    OnComplete.ExecuteIfBound(TArray<FSpatialHashQueryResult>());
    return;
}

// Query execution failure
if (!Result.bSuccess) {
    UE_LOG(LogTemp, Error, TEXT("Query failed: %s"), *Result.ErrorMessage);
    OnComplete.ExecuteIfBound(TArray<FSpatialHashQueryResult>());
    return;
}
```

## Best Practices

### 1. Capture by Value in Lambdas

When capturing variables in lambdas, be careful with object lifetimes:

```cpp
// ✅ GOOD: Capture by value (creates copies)
float Radius = 500.0f;
Manager->QueryAsync(...,
    FOnComplete::CreateLambda([Radius](const Results&) {
        // Safe to use Radius
    })
);

// ❌ BAD: Capture by reference (may be invalid when callback runs)
float Radius = 500.0f;
Manager->QueryAsync(...,
    FOnComplete::CreateLambda([&Radius](const Results&) {
        // Radius might be invalid!
    })
);
```

### 2. Handle Empty Results

Always check for empty results:

```cpp
Manager->QueryAsync(...,
    FOnComplete::CreateLambda([](const Results&) {
        if (Results.Num() == 0) {
            UE_LOG(LogTemp, Warning, TEXT("No trajectories found"));
            return;
        }
        // Process results...
    })
);
```

### 3. Use UObject Methods for Member Access

If you need to call member functions in the callback:

```cpp
// In UMyActor.h
UFUNCTION()
void OnQueryComplete(const TArray<FSpatialHashQueryResult>& Results);

// In UMyActor.cpp
void UMyActor::StartQuery() {
    Manager->QueryAsync(...,
        FOnSpatialHashQueryComplete::CreateUObject(this, &UMyActor::OnQueryComplete)
    );
}

void UMyActor::OnQueryComplete(const TArray<FSpatialHashQueryResult>& Results) {
    // Can safely access member variables here
    this->MyMemberVariable = Results.Num();
}
```

### 4. Avoid Chaining Too Many Async Calls

For complex queries requiring multiple async operations, consider using a state machine or coroutine pattern:

```cpp
// Instead of deeply nested callbacks, use a helper class
class FQueryStateMachine {
    void Step1() {
        Manager->QueryAsync(..., [this](auto Results) {
            SaveResults(Results);
            Step2();
        });
    }
    
    void Step2() {
        Manager->QueryAsync(..., [this](auto Results) {
            CombineResults(Results);
            Step3();
        });
    }
    
    // etc.
};
```

## Performance Characteristics

| Operation | Synchronous | Async |
|-----------|-------------|-------|
| **Spatial Hash Lookup** | ~1-10ms | ~1-10ms (same) |
| **File I/O** | 10-1000ms (blocks) | 10-1000ms (background) |
| **Game Thread Impact** | Blocks for entire duration | Only for hash lookup |
| **Frame Rate** | Can drop | Stays smooth |
| **Memory** | Temporary for query | Temporary for query |

## Comparison with Synchronous Methods

| Feature | Sync Methods | Async Methods |
|---------|-------------|---------------|
| **API** | LoadShardFile() directly | FTrajectoryDataCppApi |
| **Threading** | Game thread | Background thread |
| **Return Type** | int32 (count) | void (callback) |
| **Results** | Via output parameter | Via callback delegate |
| **Frame Rate** | Can stutter | Smooth |
| **Complexity** | Simple | Requires callbacks |
| **Best For** | Small datasets, immediate needs | Any size, non-urgent |

## Migration Guide

### From Synchronous to Async

**Before:**
```cpp
TArray<FSpatialHashQueryResult> Results;
int32 Count = Manager->QueryRadiusWithDistanceCheck(
    DatasetDir, QueryPos, Radius, CellSize, TimeStep, Results);

if (Count > 0) {
    ProcessResults(Results);
}
```

**After:**
```cpp
Manager->QueryRadiusWithDistanceCheckAsync(
    DatasetDir, QueryPos, Radius, CellSize, TimeStep,
    FOnSpatialHashQueryComplete::CreateLambda([this](const TArray<FSpatialHashQueryResult>& Results) {
        if (Results.Num() > 0) {
            ProcessResults(Results);
        }
    })
);
```

### Key Differences

1. **Method name**: Add `Async` suffix
2. **Return type**: `int32` → `void`
3. **Results**: Remove output parameter, use callback
4. **Processing**: Move result processing into callback

## Troubleshooting

### Callback Never Invoked

**Possible causes:**
- TrajectoryData plugin not loaded
- Invalid dataset directory
- Query failed to start (check logs)

**Solution:**
```cpp
if (!Api) {
    UE_LOG(LogTemp, Error, TEXT("TrajectoryDataCppApi not available"));
}
```

### Results Always Empty

**Possible causes:**
- No trajectories in spatial hash
- No trajectories match query criteria
- Time step out of range

**Solution:**
Add logging in callback:
```cpp
FOnComplete::CreateLambda([](const Results&) {
    UE_LOG(LogTemp, Log, TEXT("Callback invoked with %d results"), Results.Num());
})
```

### Crash in Callback

**Possible causes:**
- Invalid captured variables
- Object deleted before callback
- Thread safety issue

**Solution:**
Use weak pointers for UObject captures:
```cpp
TWeakObjectPtr<UMyActor> WeakThis(this);
FOnComplete::CreateLambda([WeakThis](const Results&) {
    if (WeakThis.IsValid()) {
        WeakThis->ProcessResults(Results);
    }
})
```

## Related Documentation

- `TrajectoryDataCppApi.h` - Async API interface
- `AVOIDING_BUSY_WAITING.md` - Why not to busy-wait
- `CORRECT_API_USAGE.md` - API integration details

## Future Enhancements

Potential improvements for future versions:

1. **Cancellation Support**: Add ability to cancel in-flight queries
2. **Progress Callbacks**: Report progress for long-running queries
3. **Batch Queries**: Queue multiple queries and process in batch
4. **Result Caching**: Cache recent query results for instant retrieval
5. **Blueprint Support**: Add latent Blueprint nodes for async queries
