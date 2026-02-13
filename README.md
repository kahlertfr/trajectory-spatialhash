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

### Basic Usage

The spatial hash table provides efficient spatial indexing for trajectory points:

```cpp
#include "SpatialHashedTrajectoryModule.h"

// The plugin provides spatial hash functionality for trajectory data
// More detailed usage examples will be added as the API is implemented
```

### Key Concepts

- **Spatial Hash**: A grid-based spatial partitioning structure that divides space into uniform cells
- **Fixed Radius Search**: Queries that find all points within a specified distance from a query point
- **Trajectory Data**: Time-series of spatial points representing object movements or paths

## API Overview

The plugin will provide the following core functionality:

- Spatial hash table creation and management
- Insertion of trajectory points
- Fixed radius nearest neighbor queries
- Efficient spatial updates and queries

*Detailed API documentation will be added as the implementation progresses.*

## Development

### Building the Plugin

The plugin will be automatically compiled when you build your Unreal Engine project. To build manually:

1. Generate project files for your Unreal Engine project
2. Build the project in your IDE (Visual Studio, Rider, etc.)

### Project Structure

```
SpatialHashedTrajectory/
├── SpatialHashedTrajectory.uplugin    # Plugin descriptor
├── README.md                           # This file
├── Resources/
│   └── Icon128.png                    # Plugin icon
└── Source/
    └── SpatialHashedTrajectory/
        ├── SpatialHashedTrajectory.Build.cs      # Build configuration
        ├── Public/
        │   └── SpatialHashedTrajectoryModule.h   # Module interface
        └── Private/
            └── SpatialHashedTrajectoryModule.cpp # Module implementation
```

## License

See LICENSE file for details.

## Author

kahlertfr

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.

## Support

For issues and questions, please use the GitHub issue tracker.
