# Spatial Hashed Trajectory

A spatial hash table implementation for Unreal Engine 5 that provides efficient fixed radius nearest neighbor searches on trajectory data.

## Description

This plugin implements a spatial hash data structure optimized for fast spatial queries on trajectory data. It enables efficient fixed-radius nearest neighbor searches, which are essential for trajectory analysis, collision detection, and spatial queries in simulations and visualizations.

## Features

- **Spatial Hash Table**: Efficient spatial indexing data structure
- **Fixed Radius Queries**: Fast nearest neighbor searches within a specified radius
- **Trajectory Integration**: Designed to work seamlessly with trajectory data
- **Runtime Module**: Lightweight runtime-only module with minimal overhead

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
- Supports multiple trajectory data file formats:
  - **Binary files** (.bin): Trajectory shard files with header (magic number, version, etc.)
  - **Text files** (.csv, .dat): CSV format with columns: TimeStep,TrajectoryID,X,Y,Z
- The system automatically searches the dataset directory for trajectory data files
- Filters out existing spatial hash table files to avoid conflicts
- If hash tables already exist, they are simply loaded from disk
- Set `Auto Create` to `false` to only load existing hash tables without attempting creation

**Trajectory Data Format:**

Binary format:
```
Header (64 bytes): Magic (4) + Version (4) + NumTrajectories (4) + NumTimeSteps (4) + Reserved (48)
Per TimeStep: NumSamples (4) + [TrajectoryID (4) + X (4) + Y (4) + Z (4)] × NumSamples
```

Text/CSV format:
```csv
TimeStep,TrajectoryID,X,Y,Z
0,1,10.5,20.3,5.0
0,2,15.2,22.1,5.5
...
```

**Note:** The Dataset Directory should be an absolute filesystem path, not a Content Browser path. Use Unreal's path functions like `FPaths::ProjectContentDir()` to construct the correct path.

#### Querying Nearest Neighbors

To find all trajectories within a specific radius:

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

The plugin provides functionality to build spatial hash tables from trajectory data:

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

// Build hash tables
FSpatialHashTableBuilder Builder;
if (Builder.BuildHashTables(Config, TimeStepSamples))
{
    UE_LOG(LogTemp, Log, TEXT("Successfully built spatial hash tables"));
}
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
- **Query Fixed Radius Neighbors**: Find all trajectories within a radius at a specific time step
- **Query Cell**: Get all trajectories in the same cell as a query position
- **Memory Management**: Check loaded hash tables, get memory stats, and unload hash tables
- **Multiple Cell Sizes**: Manage and query hash tables with different cell sizes simultaneously

### Spatial Hash Table (`FSpatialHashTable`) - C++ API
- Load hash tables from binary files with `LoadFromFile()` (trajectory IDs not loaded for memory optimization)
- Save hash tables to binary files with `SaveToFile()`
- Query trajectories at world positions with `QueryAtPosition()` (reads trajectory IDs from disk on-demand)
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
        │   └── SpatialHashTableExample.h              # Example usage and validation
        └── Private/
            ├── SpatialHashedTrajectoryModule.cpp      # Module implementation
            ├── SpatialHashTable.cpp                   # Hash table implementation
            ├── SpatialHashTableBuilder.cpp            # Builder implementation
            └── SpatialHashTableManager.cpp            # Manager implementation
```

## License

See LICENSE file for details.

## Author

kahlertfr

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For issues and questions, please use the GitHub issue tracker.
