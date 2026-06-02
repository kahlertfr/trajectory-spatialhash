# Spatial Hashed Trajectory

A spatial hash table implementation for Unreal Engine 5 that provides efficient fixed radius nearest neighbor searches on trajectory data.

## Description

This plugin implements a spatial hash data structure optimized for fast spatial queries on trajectory data. It enables efficient fixed-radius nearest neighbor searches, which are essential for trajectory analysis, collision detection, and spatial queries in simulations and visualizations.

## Features

- **Spatial Hash Table**: Efficient spatial indexing data structure
- **Fixed Radius Queries**: Fast nearest neighbor searches within a specified radius
- **Distance-Verified Queries**: Actual distance calculations using TrajectoryData plugin
- **Multiple Query Types**: Support for single point, time range, and trajectory-based queries
- **Dual Radius Queries**: Simultaneous inner/outer radius queries for memory efficiency
- **Trajectory Integration**: Designed to work seamlessly with trajectory data
- **Runtime Module**: Lightweight runtime-only module with minimal overhead
- **🆕 Async Query Support**: Non-blocking queries using TrajectoryData plugin's async API
- **🆕 Blueprint Async Task Nodes**: Blueprint-accessible async task nodes for smooth performance

## New: Async Query Methods (C++ and Blueprint)

The plugin now supports fully asynchronous queries that don't block the game thread, available in both C++ and Blueprint:

**C++ Example:**
```cpp
// Async query with callback - NO BUSY WAITING!
Manager->QueryRadiusWithDistanceCheckAsync(
    DatasetDirectory, QueryPosition, Radius, CellSize, TimeStep,
    FOnSpatialHashQueryComplete::CreateLambda([](const TArray<FSpatialHashQueryResult>& Results) {
        UE_LOG(LogTemp, Log, TEXT("Found %d trajectories"), Results.Num());
        // Process results here - callback invoked on game thread when ready
    })
);
// Game thread continues immediately - smooth frame rate!
```

**Blueprint Example:**
```
Call "Query Radius Async" (Blueprint Async Action)
├─ Returns async task node with output execution pins
├─ "On Complete" pin fires when results ready
└─ Results parameter contains query results
```

**Benefits:**
- ✅ Non-blocking - game thread continues during I/O
- ✅ Smooth frame rate even with large datasets
- ✅ Proper async callbacks (no busy-waiting)
- ✅ Thread-safe result delivery on game thread
- ✅ Available in both C++ and Blueprint

**Available Async Methods:**
- C++: `QueryRadiusWithDistanceCheckAsync()`, `QueryDualRadiusWithDistanceCheckAsync()`, `QueryRadiusOverTimeRangeAsync()`, `QueryTrajectoryRadiusOverTimeRangeAsync()`
- Blueprint: `QueryRadiusAsync`, `QueryDualRadiusAsync`, `QueryTimeRangeAsync`, `QueryTrajectoryAsync`

📖 **See [ASYNC_QUERY_METHODS.md](ASYNC_QUERY_METHODS.md) for complete C++ documentation and examples!**
📖 **See README section "Async Queries (Blueprint)" for Blueprint usage examples.**

## Dependencies

This plugin requires the following dependencies:

### Required Plugins

- **TrajectoryData** - Provides trajectory data structures and utilities
  - Repository: [kahlertfr/ue-plugin-trajectory-data](https://github.com/kahlertfr/ue-plugin-trajectory-data)
  - This plugin must be installed and enabled in your Unreal Engine project

### Engine Requirements

- Unreal Engine 5.0 or later

## Installation

1. **Install TrajectoryData dependency:**
   ```bash
   # Clone the TrajectoryData plugin into your project's Plugins folder
   cd YourProject/Plugins
   git clone https://github.com/kahlertfr/ue-plugin-trajectory-data.git TrajectoryData
   ```

2. **Install SpatialHashedTrajectory:**
   ```bash
   # Clone this plugin into your project's Plugins folder
   cd YourProject/Plugins
   git clone https://github.com/kahlertfr/trajectory-spatialhash.git SpatialHashedTrajectory
   ```

3. **Enable the plugins:**
   - Open your Unreal Engine project
   - Go to Edit → Plugins
   - Search for "Spatial Hashed Trajectory" and enable it
   - The TrajectoryData plugin should be automatically enabled as a dependency
   - Restart the editor when prompted

4. **Add to your module's Build.cs** (if using in C++ code):
   ```csharp
   PublicDependencyModuleNames.AddRange(new string[] { 
       "Core", 
       "CoreUObject", 
       "Engine",
       "SpatialHashedTrajectory"
   });
   ```

## Usage

### Using from Blueprints

The plugin provides a Blueprint-accessible manager class for loading and querying spatial hash tables:

#### Creating a Hash Table Manager

1. In your Blueprint, create a variable of type `Spatial Hash Table Manager`
2. Initialize it in your BeginPlay event or similar initialization point

#### Loading Hash Tables (with Auto-Creation)

The manager can automatically create hash tables if they don't exist:

```
Event BeginPlay
├─ Create Object of Class (Spatial Hash Table Manager)
│  └─ Assign to Manager variable
└─ Call "Load Hash Tables" on Manager
   ├─ Dataset Directory: Use FPaths::ProjectContentDir() + "Data/Trajectories" or absolute path
   ├─ Cell Size: 10.0
   ├─ Start Time Step: 0
   ├─ End Time Step: 100
   └─ Auto Create: true (default - creates hash tables if they don't exist)
```

**Auto-Creation Behavior:**
- If `Auto Create` is `true` (default) and hash tables don't exist for the requested cell size, the manager will automatically create them from trajectory data
- **Important:** Hash tables are now built from the **COMPLETE dataset** - all shards and all trajectories are processed
- **Note:** Auto-creation in `LoadHashTables` runs synchronously and will block until complete
- For non-blocking creation, use `CreateHashTablesAsync` (see below)
- Trajectory data is loaded using the **TrajectoryData plugin's `LoadShardFile` function** (https://github.com/kahlertfr/ue-plugin-trajectory-data)
- Supports the full trajectory data format specification:
  - `dataset-manifest.json` - Dataset metadata (scenario name, physical times, units)
  - `dataset-meta.bin` - Binary metadata
  - `dataset-trajmeta.bin` - Per-trajectory metadata
  - `shard-*.bin` - Time-interval data files (**ALL shards are processed**)
- **Performance:** Shards are processed in parallel using multiple CPU cores
- **Memory Management:** Memory usage is proportional to the number of shards and trajectory entries
- If hash tables already exist, they are simply loaded from disk
- Set `Auto Create` to `false` to only load existing hash tables without attempting creation
- See [trajectory data specification](https://github.com/kahlertfr/ue-plugin-trajectory-data/blob/main/specification-trajectory-data-shard.md) for data format details
- See [LoadShardFile documentation](https://github.com/kahlertfr/ue-plugin-trajectory-data/blob/main/LOAD_SHARD_FILE.md) for details on shard loading

#### Creating Hash Tables Asynchronously (Non-Blocking)

For large datasets, use async creation to avoid blocking the game thread:

```
On Button Click or Custom Event
├─ Call "Create Hash Tables Async" on Manager
│  ├─ Dataset Directory: Path to trajectory data
│  ├─ Cell Size: 10.0
│  ├─ Start Time Step: (ignored - all time steps are processed)
│  └─ End Time Step: (ignored - all time steps are processed)
└─ Optional: Poll "Is Creating Hash Tables" to check progress
   └─ Branch: If False, creation is complete
```

**Async Creation Benefits:**
- **Non-blocking:** Returns immediately, game thread stays responsive
- **Parallelized:** 
  - Shards are processed in parallel
  - Time steps within shards are also processed in parallel
  - Two-level parallelism for maximum performance
- **Complete dataset:** Processes ALL trajectories from ALL shards
- **Performance:** Significant speedup for large datasets with multiple shards
- Can monitor progress with `Is Creating Hash Tables` node

**Important Notes:**
- The `Start Time Step` and `End Time Step` parameters are **ignored** in the new implementation
- Hash tables are built for the **complete time range** of the dataset (all time steps in all shards)
- This ensures hash tables represent the full spatial distribution of the data

**Trajectory Data Format:**

This plugin uses the **TrajectoryData plugin's LoadShardFile** for loading complete shard data. The dataset directory should contain:
- `dataset-manifest.json` - Human-readable metadata (scenario name, physical times, coordinate units)
- `dataset-meta.bin` - Binary dataset metadata (time step ranges, bounding box, etc.)
- `dataset-trajmeta.bin` - Per-trajectory metadata
- `shard-*.bin` - Time-interval data files with trajectory positions (**ALL shards are loaded and processed**)

Each shard file is loaded completely using `LoadShardFile`, which provides structured access to all trajectory entries in the shard. This ensures:
- Complete dataset coverage (no trajectory filtering)
- All trajectories are included in hash tables
- Spatial distribution accurately represents the full dataset

See the [trajectory data specification](https://github.com/kahlertfr/ue-plugin-trajectory-data/blob/main/specification-trajectory-data-shard.md) for detailed format information and [LoadShardFile documentation](https://github.com/kahlertfr/ue-plugin-trajectory-data/blob/main/LOAD_SHARD_FILE.md) for shard loading details.

**Note:** The Dataset Directory should be an absolute filesystem path, not a Content Browser path. Use Unreal's path functions like `FPaths::ProjectContentDir()` to construct the correct path.

#### Querying Nearest Neighbors

The plugin now supports advanced nearest neighbor queries with actual distance calculations:

**Basic Query (Legacy - returns trajectory IDs only):**
```
On Some Event
└─ Call "Query Fixed Radius Neighbors" on Manager
   ├─ Query Position: (X, Y, Z) - Your query location
   ├─ Radius: 50.0 - Search radius in world units
   ├─ Cell Size: 10.0 - Must match loaded hash tables
   ├─ Time Step: Current time step
   └─ Out Results: Array of trajectory IDs and distances
      └─ ForEach Loop
         └─ Do something with each result
            ├─ Trajectory ID
            └─ Distance
```

**Advanced Queries with Distance Verification:**

For queries with actual distance calculations using TrajectoryData, see the [Nearest Neighbor Query Guide](NEAREST_NEIGHBOR_QUERY_GUIDE.md) for detailed examples.

#### Sample Niagara Visualization Actor

Use `ASampleTrajectoryNiagaraActor` to showcase path view (world-space trajectories) and object-centered view (trajectories aligned to the moving object's frame) without running any spatial hash queries.

**Placement steps:**
1. In the level editor, add **SampleTrajectoryNiagaraActor** (Place Actors → All Classes → SampleTrajectoryNiagaraActor).
2. Assign the same **Niagara System** asset used by `ATrajectoryQueryNiagaraActor` to the actor's **NiagaraSystem** property.
3. Pick the **SelectedScenario** (A–F) in the Details panel, or call `SetSelectedScenario` at runtime.
4. (Optional) Assign a **UserSelectionManager** to have time range / radius / color updates pushed to Niagara.
   - The actor looks for a selection whose `TrajectoryId` matches the scenario index (A=0 ... F=5).
   - If none match, it applies the first available selection.

**Generated data defaults:**
- Periodic volume extent: **5×5×5**
- Time steps: **360**
- Query object radius: **0.5**
- Inner radius: **0.6**
- Particle radius: **0.01**

**Query Types:**
- **Single Point, Single Timestep (Case A)**: Returns one sample per trajectory within radius at a specific time
- **Single Point, Time Range (Case B)**: Returns trajectories with samples within radius over a time range
- **Trajectory Query (Case C)**: Returns trajectories that intersect with a moving query trajectory
- **Dual Radius**: Simultaneously query inner and outer radius for memory efficiency

Example:
```
On Some Event
└─ Call "Query Radius With Distance Check" on Manager
   ├─ Dataset Directory: Path to trajectory data
   ├─ Query Position: (X, Y, Z)
   ├─ Radius: 50.0
   ├─ Cell Size: 10.0
   ├─ Time Step: Current time step
   └─ Out Results: Array of FSpatialHashQueryResult
      └─ ForEach Loop
         └─ Process trajectory with full sample data
            ├─ Trajectory ID
            └─ Sample Points (with positions, distances, timesteps)
```

#### Querying a Single Cell

To get all trajectories in the same cell as a position:

```
On Some Event
└─ Call "Query Cell" on Manager
   ├─ Query Position: (X, Y, Z)
   ├─ Cell Size: 10.0
   ├─ Time Step: Current time step
   └─ Out Trajectory IDs: Array of trajectory IDs
      └─ ForEach Loop
         └─ Process each trajectory ID
```

#### User Selection Manager (Blueprint)

The `UUserSelectionManager` class lets you store a list of user-driven trajectory selections and automatically push updates to any Blueprint that registers interest.  It supports two complementary notification patterns — a **delegate binding** for quick one-off handlers and a formal **listener interface** for objects that need a persistent callback — and a separate **provider interface** so Blueprints can contribute selections rather than just consume them.

---

##### The Selection Data Structure

Each entry in the manager is an `FUserTrajectorySelection` struct with ten fields:

| Field | Type | Description |
|---|---|---|
| `TrajectoryId` | `int32` | ID of the selected trajectory |
| `TimeStart` | `int32` | First time step of the selection (inclusive) |
| `TimeEnd` | `int32` | Last time step of the selection (inclusive) |
| `TimeRangeSensitive` | `bool` | Whether this selection should be sensitive to the selected time range |
| `InnerRadius` | `float` | Inner query radius for dual-radius lookups |
| `OuterRadius` | `float` | Outer query radius for dual-radius lookups |
| `CollisionRadius` | `float` | Collision query radius |
| `ColorEncoding` | `ETrajectoryColorEncoding` | Color mode (`Time`, `Distance`, `DistanceCategory`, `VolumeImage`, `Velocity`, `SelectorHighlight`) |
| `DistanceFilter` | `ETrajectoryDistanceFilter` | Distance filter (`NoFilter`, `ReachesInnerRadius`, `DoesNotReachInnerRadius`, `Collision`) |
| `bParticlesVisible` | `bool` | Whether particles are visible for the selected trajectory |
| `NoShadow` | `bool` | Whether the selected trajectory is rendered without shadow contribution |

---

##### Step 1 – Create and store the manager

Create exactly one `UserSelectionManager` object, typically in your **Game Mode** or **Game State**, and store it in a variable so every other Blueprint can reach it.

```
Event BeginPlay
└─ Create Object of Class: User Selection Manager
   └─ Assign to "SelectionManager" variable (expose as Blueprint readable)
```

---

##### Step 2 – Reading and mutating the selection list

Use the manager's access and mutation nodes any time the user picks, changes, or removes a trajectory selection.  Every mutation call automatically notifies all registered listeners.

```
// Add a new selection
SelectionManager → Add Selection
   ├─ Trajectory Id: 42
   ├─ Time Start: 0
   ├─ Time End: 500
   ├─ Inner Radius: 25.0
   ├─ Outer Radius: 50.0
   ├─ Color Encoding: Time
   ├─ Distance Filter: No Filter
   └─ Particles Visible: true

// Replace a selection at a known index (returns false if index is out of range)
SelectionManager → Update Selection
   ├─ Index: 0
   └─ Selection: (new FUserTrajectorySelection values)

// Replace the selection whose TrajectoryId matches (returns false if not found)
SelectionManager → Update Selection By Trajectory Id
   ├─ Trajectory Id: 42
   └─ Selection: (new FUserTrajectorySelection values)

// Remove the selection at an index (returns false if index is out of range)
SelectionManager → Remove Selection
   └─ Index: 0

// Wipe the whole list
SelectionManager → Clear Selections

// Read all current selections at any time (pure, no side effects)
SelectionManager → Get Selections → TArray<FUserTrajectorySelection>

// Look up a single selection by trajectory ID (pure, no side effects)
// Returns true + filled OutSelection when found, false when not found
SelectionManager → Get Selection By Trajectory Id
   ├─ Trajectory Id: 42
   ├─ Return value (bool): true if found
   └─ Out Selection: FUserTrajectorySelection
```

---

##### Step 3 – Reacting to changes (Listener pattern)

There are two equivalent ways to be notified.  Pick whichever fits your Blueprint's structure.

**Option A – Delegate binding (simplest)**

Bind directly to `OnSelectionsChanged` from any Blueprint.  No interface is required.

```
Event BeginPlay (inside the Blueprint that wants to react)
└─ Bind Event to On Selections Changed on SelectionManager
   └─ Target: SelectionManager
      └─ Event: [Custom Event] Handle Selections Changed

[Custom Event] Handle Selections Changed
   └─ Selections: TArray<FUserTrajectorySelection>
      └─ ForEach Loop
         └─ Break FUserTrajectorySelection
            ├─ Trajectory Id  → use in your logic
            ├─ Time Start     → use in your logic
            ├─ Time End       → use in your logic
            ├─ Inner Radius   → use in your logic
            ├─ Outer Radius   → use in your logic
            ├─ Color Encoding → use in your logic
            ├─ Distance Filter → use in your logic
            └─ Particles Visible → use in your logic
```

**Option B – Listener interface (persistent, multi-actor)**

Implement the `Trajectory Selection Listener` interface in any Blueprint Actor or Object class.  Override the `On User Selections Changed` event that the interface provides, then register with the manager.

```
// In your Blueprint class settings → Interfaces → add "Trajectory Selection Listener"

// Override the interface event:
Event On User Selections Changed (Selections: TArray<FUserTrajectorySelection>)
   └─ [your update logic here]

// Register once at startup:
Event BeginPlay
└─ SelectionManager → Register Listener
   └─ Listener: Self

// Unregister when the actor is done (optional – stale listeners are cleaned up automatically):
Event EndPlay
└─ SelectionManager → Unregister Listener
   └─ Listener: Self
```

> **Tip:** You can use both patterns simultaneously.  A UI widget might use the delegate binding while a visualizer actor uses the interface.

---

##### Step 4 – Contributing selections (Provider pattern)

If a Blueprint *produces* selections (e.g. a UI picker, a VR controller, or an analysis actor) instead of just reacting to them, implement the `Trajectory Selection Provider` interface and let the manager aggregate all providers automatically.

```
// In your Blueprint class settings → Interfaces → add "Trajectory Selection Provider"

// Implement the interface function:
Function Get Provided Selections → TArray<FUserTrajectorySelection>
   ├─ Make FUserTrajectorySelection (Trajectory Id: ..., Time Start: ..., ...)
   └─ Return Array containing all selections this actor currently owns

// Register with the manager at startup:
Event BeginPlay
└─ SelectionManager → Register Provider
   └─ Provider: Self

// Unregister when done:
Event EndPlay
└─ SelectionManager → Unregister Provider
   └─ Provider: Self
```

Call `RefreshFromProviders` whenever the manager should re-poll every registered provider and merge their results into a fresh selection list.  This fires `OnSelectionsChanged` exactly once after all providers have been queried.

```
// Trigger a full refresh (e.g. from a "Confirm Selection" button):
SelectionManager → Refresh From Providers
```

---

##### Complete example – UI + Visualizer + Provider

This end-to-end example shows a typical setup with three Blueprints:

**BP_GameMode** – owns the manager

```
Event BeginPlay
└─ Create Object: User Selection Manager → assign to "SelectionManager"
```

**BP_UIWidget** (implements `ITrajectorySelectionProvider`) – lets the user pick trajectories

```
// Interface: ITrajectorySelectionProvider
Function Get Provided Selections → TArray<FUserTrajectorySelection>
   └─ Return: [SelectedItems array built from the widget's list view]

Event On Button Confirm Clicked
├─ GameMode → SelectionManager → Register Provider (Self)   // safe to call repeatedly
└─ GameMode → SelectionManager → Refresh From Providers     // notifies all listeners
```

**BP_TrajectoryVisualizer** (implements `ITrajectorySelectionListener`) – reacts to changes

```
// Interface: ITrajectorySelectionListener
Event On User Selections Changed (Selections)
   └─ ForEach Selections
      └─ Start visualization for each FUserTrajectorySelection

Event BeginPlay
└─ GameMode → SelectionManager → Register Listener (Self)
```

When the user clicks **Confirm**, the manager polls `BP_UIWidget::GetProvidedSelections`, stores the result, and calls `BP_TrajectoryVisualizer::OnUserSelectionsChanged` — all with no manual wiring between the two actors.

---

#### Async Queries (Blueprint - Recommended)

**⚠️ IMPORTANT: Use Async Methods to Prevent Game Thread Blocking**

The synchronous query methods (`Query Radius With Distance Check`, etc.) can block the game thread during disk I/O, causing frame rate stutters. For smooth performance, use the async Blueprint task nodes instead:

**Async Query Benefits:**
- ✅ **Non-blocking** - Game thread continues during data loading
- ✅ **Smooth frame rate** - No stutters even with large datasets
- ✅ **Event-driven** - Results delivered via Blueprint output pins
- ✅ **Production-ready** - Safe for shipping games

**Available Async Blueprint Task Nodes:**

1. **Query Radius Async** - Single point, single timestep
2. **Query Dual Radius Async** - Inner/outer radius simultaneously
3. **Query Time Range Async** - Single point over time range
4. **Query Trajectory Async** - Trajectory interaction queries

**Example - Async Single Point Query:**
```
Event Tick or Custom Event
├─ Call "Query Radius Async" (returns Async Task)
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Manager
│  ├─ Dataset Directory: Path to trajectory data
│  ├─ Query Position: (X, Y, Z)
│  ├─ Radius: 50.0
│  ├─ Cell Size: 10.0
│  └─ Time Step: Current time step
├─ Bind to "On Complete" output pin
│  └─ Execution continues when results ready
│     ├─ Results: Array of Spatial Hash Query Result
│     └─ ForEach Loop over Results
│        └─ Process each trajectory:
│           ├─ Trajectory ID
│           └─ Sample Points (positions, distances, timesteps)
└─ Other game logic continues immediately (async node returns right away)
```

**Example - Async Dual Radius Query:**
```
On Some Event
├─ Call "Query Dual Radius Async" (returns Async Task)
│  ├─ World Context Object: Self
│  ├─ Manager: Your Spatial Hash Manager
│  ├─ Dataset Directory: Path to trajectory data
│  ├─ Query Position: (X, Y, Z)
│  ├─ Inner Radius: 20.0
│  ├─ Outer Radius: 50.0
│  ├─ Cell Size: 10.0
│  └─ Time Step: Current time step
├─ Bind to "On Complete" output pin
│  └─ Execution continues when results ready
│     ├─ Inner Results: Array of Spatial Hash Query Result
│     ├─ Outer Results: Array of Spatial Hash Query Result
│     └─ Process both result sets
└─ Other game logic continues immediately
```

**When to Use Each Query Type:**
- **Async Task Nodes** (Recommended): Production use, smooth frame rate required
- **Sync methods**: Quick prototyping, editor tools, or when blocking is acceptable
- **C++ Async methods**: When you need more control over callbacks

**Performance Tips:**
- Async task nodes use background threads for I/O - no game thread blocking
- Results are automatically delivered on the game thread via execution pins
- Multiple async queries can run simultaneously
- No busy-waiting loops - proper event-driven architecture

See [ASYNC_QUERY_METHODS.md](ASYNC_QUERY_METHODS.md) for complete documentation and more examples.

#### Managing Memory

Check what's loaded and manage memory:

```
// Get loaded cell sizes
Call "Get Loaded Cell Sizes" → Returns array of cell sizes

// Get loaded time steps for a cell size
Call "Get Loaded Time Steps" → Returns array of time steps

// Check if specific hash table is loaded
Call "Is Hash Table Loaded" → Returns true/false

// Get memory statistics
Call "Get Memory Stats" → Returns total hash tables and memory usage

// Unload specific cell size
Call "Unload Hash Tables" with Cell Size: 10.0

// Unload everything
Call "Unload All Hash Tables"
```

### Building Spatial Hash Tables (C++)

The plugin provides functionality to build spatial hash tables from trajectory data. **Note:** The recommended approach is to use `USpatialHashTableManager` which now uses `LoadShardFile` to process complete datasets.

**Using the Manager (Recommended):**

```cpp
#include "SpatialHashTableManager.h"

// Create manager
USpatialHashTableManager* Manager = NewObject<USpatialHashTableManager>();

// Create hash tables from complete dataset (processes all shards)
Manager->CreateHashTablesAsync(
    TEXT("/Path/To/Dataset"),
    10.0f,  // Cell size
    0,      // Start time step (ignored - all time steps processed)
    0);     // End time step (ignored - all time steps processed)

// Or synchronously
bool bSuccess = Manager->TryCreateHashTables(
    TEXT("/Path/To/Dataset"),
    10.0f,  // Cell size  
    0,      // Start time step (ignored)
    0);     // End time step (ignored)
```

**Direct Builder API:**

If you already have trajectory samples prepared in memory:

```cpp
#include "SpatialHashTableBuilder.h"

// Configure the builder
FSpatialHashTableBuilder::FBuildConfig Config;
Config.CellSize = 10.0f;  // 10 unit cells
Config.bComputeBoundingBox = true;  // Automatically compute bounding box
Config.OutputDirectory = TEXT("/Path/To/Dataset");
Config.NumTimeSteps = 1000;  // Number of time steps to process

// Prepare trajectory samples for each time step
TArray<TArray<FSpatialHashTableBuilder::FTrajectorySample>> TimeStepSamples;
// ... populate TimeStepSamples with trajectory data ...

// Build hash tables (processes time steps in parallel)
FSpatialHashTableBuilder Builder;
if (Builder.BuildHashTables(Config, TimeStepSamples))
{
    UE_LOG(LogTemp, Log, TEXT("Successfully built spatial hash tables"));
}
```

**Key Implementation Details:**
- Hash tables are built from **ALL trajectories** in the dataset
- Uses `LoadShardFile` from TrajectoryData plugin for complete shard data
- Shards are processed in parallel for better performance
- Time steps are also processed in parallel
- No trajectory filtering or selection is applied
```

### Loading and Querying Hash Tables

Load a hash table from disk and query trajectory IDs at a specific position:

```cpp
#include "SpatialHashTable.h"

// Load hash table for a specific time step
FSpatialHashTable HashTable;
FString Filename = TEXT("/Path/To/Dataset/spatial_hashing/cellsize_10.000/timestep_00000.bin");

if (HashTable.LoadFromFile(Filename))
{
    // Query trajectories at a specific world position
    TArray<uint32> TrajectoryIds;
    FVector QueryPosition(100.0f, 200.0f, 50.0f);
    
    if (HashTable.QueryAtPosition(QueryPosition, TrajectoryIds))
    {
        UE_LOG(LogTemp, Log, TEXT("Found %d trajectories at position"), TrajectoryIds.Num());
        for (uint32 Id : TrajectoryIds)
        {
            // Process trajectory ID...
        }
    }
}
```

### File Organization

The plugin creates the following directory structure for hash table files:

```
<dataset-directory>/
└── spatial_hashing/
    ├── cellsize_10.000/
    │   ├── timestep_00000.bin
    │   ├── timestep_00001.bin
    │   └── ...
    ├── cellsize_5.000/
    │   ├── timestep_00000.bin
    │   └── ...
    └── ...
```

Each `.bin` file contains a complete hash table for one time step at a specific cell size, formatted for efficient memory-mapped loading.

### Key Concepts

- **Spatial Hash**: A grid-based spatial partitioning structure that divides space into uniform cells using Z-Order curves (Morton codes) for spatial indexing
- **Cell Size**: The size of each spatial cell in world units. Smaller cells provide finer spatial resolution but require more memory
- **Z-Order Curve**: A space-filling curve that maps 3D coordinates to a single dimension while preserving spatial locality
- **Time Step**: Each hash table represents trajectory positions at a single point in time
- **Binary Format**: Hash tables are stored in a binary format optimized for memory-mapped loading without parsing
- **On-Demand Loading**: Trajectory IDs are read from disk only when queried, not loaded into memory, significantly reducing memory usage when managing multiple hash tables
- **Two-Phase Queries**: Advanced queries use spatial hash for candidate selection, then TrajectoryData for precise distance verification
- **Distance Verification**: Actual position data loaded from TrajectoryData plugin ensures accurate results within query radius

### Memory Optimization

The plugin uses an on-demand loading strategy to minimize memory usage:

- **Header and Entries**: Loaded into memory (typically small - header is 64 bytes, entries are 16 bytes each)
- **Trajectory IDs**: NOT loaded into memory. Read from disk on each query
- **Memory Savings**: For a hash table with 1 million trajectory IDs, this saves ~4MB per loaded table
- **Scalability**: Load hundreds of time steps without memory concerns

This design allows you to manage large datasets with many time steps and cell sizes without consuming excessive memory. The trade-off is a small I/O cost per query, which is typically negligible compared to the memory savings.

## API Overview

The plugin provides the following core functionality:

### Spatial Hash Table Manager (`USpatialHashTableManager`) - Blueprint Accessible
- **Load Hash Tables**: Load hash tables from disk for a specific cell size and time range
- **Query Fixed Radius Neighbors**: Find all trajectories within a radius at a specific time step (trajectory IDs only)
- **Query Radius With Distance Check**: Find trajectories with actual distance verification (returns full sample data)
- **Query Dual Radius With Distance Check**: Query inner and outer radius simultaneously
- **Query Radius Over Time Range**: Find trajectories within radius across multiple timesteps
- **Query Trajectory Radius Over Time Range**: Find trajectories intersecting with a moving query trajectory
- **Query Cell**: Get all trajectories in the same cell as a query position
- **Memory Management**: Check loaded hash tables, get memory stats, and unload hash tables
- **Multiple Cell Sizes**: Manage and query hash tables with different cell sizes simultaneously

### Spatial Hash Table (`FSpatialHashTable`) - C++ API
- Load hash tables from binary files with `LoadFromFile()` (trajectory IDs not loaded for memory optimization)
- Save hash tables to binary files with `SaveToFile()`
- Query trajectories at world positions with `QueryAtPosition()` (reads trajectory IDs from disk on-demand)
- Query trajectory IDs in radius with `QueryTrajectoryIdsInRadius()` (candidate selection for distance verification)
- Calculate Z-Order keys for spatial cells
- Binary search for efficient cell lookups
- Memory-efficient: only header and entries loaded; trajectory IDs read on-demand

### Spatial Hash Table Builder (`FSpatialHashTableBuilder`) - C++ API
- Build hash tables from trajectory data with `BuildHashTables()`
- Automatically compute bounding boxes from data
- Support for custom bounding boxes
- Configurable cell sizes
- Automatic directory structure creation

For detailed information about the binary file format, see [specification-spatial-hash-table.md](specification-spatial-hash-table.md).

For comprehensive examples of distance-verified queries, see [NEAREST_NEIGHBOR_QUERY_GUIDE.md](NEAREST_NEIGHBOR_QUERY_GUIDE.md).

## Development

### Building the Plugin

The plugin will be automatically compiled when you build your Unreal Engine project. To build manually:

1. Generate project files for your Unreal Engine project
2. Build the project in your IDE (Visual Studio, Rider, etc.)

### Project Structure

```
SpatialHashedTrajectory/
├── SpatialHashedTrajectory.uplugin           # Plugin descriptor
├── README.md                                  # This file
├── specification-spatial-hash-table.md       # Binary format specification
├── Resources/
│   └── Icon128.png                           # Plugin icon
└── Source/
    └── SpatialHashedTrajectory/
        ├── SpatialHashedTrajectory.Build.cs           # Build configuration
        ├── Public/
        │   ├── SpatialHashedTrajectoryModule.h        # Module interface
        │   ├── SpatialHashTable.h                     # Hash table data structure
        │   ├── SpatialHashTableBuilder.h              # Hash table builder
        │   ├── SpatialHashTableManager.h              # Blueprint-accessible manager
        │   ├── SpatialHashTableExample.h              # Example usage and validation
        │   ├── UserSelectionManager.h                 # User selection store + FUserTrajectorySelection struct
        │   ├── TrajectorySelectionListener.h          # ITrajectorySelectionListener Blueprint interface
        │   └── TrajectorySelectionProvider.h          # ITrajectorySelectionProvider Blueprint interface
        └── Private/
            ├── SpatialHashedTrajectoryModule.cpp      # Module implementation
            ├── SpatialHashTable.cpp                   # Hash table implementation
            ├── SpatialHashTableBuilder.cpp            # Builder implementation
            ├── SpatialHashTableManager.cpp            # Manager implementation
            └── UserSelectionManager.cpp               # User selection manager implementation
```

## License

See LICENSE file for details.

## Author

kahlertfr

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For issues and questions, please use the GitHub issue tracker.
