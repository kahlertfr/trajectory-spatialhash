# trajectory-spatialhash

A high-performance C++ library for building spatial hash grids from trajectory data, enabling fast nearest neighbor and radius queries. Designed for use in both standalone applications and as an Unreal Engine plugin.

## Features

- 🚀 **Fast spatial queries**: Efficiently find nearest neighbors and points within a radius
- 📦 **Multiple interfaces**: C++ API, C ABI wrapper, and Unreal Engine Blueprint support
- 🔧 **Cross-platform**: Windows (MSVC), macOS, and Linux support
- 🎮 **Unreal Engine ready**: Includes plugin example with prebuilt libs and source-in-plugin approaches
- 🧪 **Well tested**: Comprehensive unit tests with Catch2
- 📖 **CMake integration**: Modern CMake with `find_package` support

## Quick Start

### Building from Source

```bash
# Clone the repository
git clone https://github.com/kahlertfr/trajectory-spatialhash.git
cd trajectory-spatialhash

# Configure and build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .

# Run tests
ctest --output-on-failure

# Install (optional)
cmake --install . --prefix /usr/local
```

### Using the CLI Tool

Build a spatial hash grid from trajectory shard CSV files:

```bash
# Build grid from one or more CSV shard files
./tsh build shard1.csv shard2.csv -o output.grid -c 10.0

# Arguments:
#   build           Command to build a grid
#   shard*.csv      Input trajectory shard files (CSV format: x,y,z,trajectory_id,point_index)
#   -o output.grid  Output binary grid file
#   -c 10.0         Cell size (optional, default: 10.0)
```

### Using as a Library

#### C++ API

```cpp
#include "trajectory_spatialhash/spatial_hash.h"

using namespace trajectory_spatialhash;

// Create a grid
SpatialHashGrid grid(10.0); // cell size = 10.0

// Build from shards
const char* shards[] = {"shard1.csv", "shard2.csv"};
grid.BuildFromShards(shards, 2);

// Find nearest point
Point3D nearest;
if (grid.Nearest(0.0, 0.0, 0.0, nearest)) {
    std::cout << "Nearest point: (" << nearest.x << ", " << nearest.y << ", " << nearest.z << ")\n";
}

// Radius query
QueryResult result;
if (grid.RadiusQuery(0.0, 0.0, 0.0, 5.0, result)) {
    std::cout << "Found " << result.count << " points within radius\n";
    SpatialHashGrid::FreeQueryResult(result);
}

// Serialize for later use
grid.Serialize("grid.bin");

// Deserialize
SpatialHashGrid loaded(10.0);
loaded.Deserialize("grid.bin");
```

#### C API (Unreal Engine safe)

```c
#include "trajectory_spatialhash/ue_wrapper.h"

// Create grid
TSH_Handle handle = TSH_Create(10.0);

// Build from shards
const char* shards[] = {"shard1.csv"};
TSH_BuildFromShards(handle, shards, 1);

// Query nearest
TSH_Point point;
if (TSH_QueryNearest(handle, 0.0, 0.0, 0.0, &point)) {
    printf("Found point at (%f, %f, %f)\n", point.x, point.y, point.z);
}

// Clean up
TSH_Free(handle);
```

### CMake Integration

After installing the library, use it in your CMake project:

```cmake
find_package(trajectory_spatialhash REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE trajectory_spatialhash::trajectory_spatialhash)
```

## Unreal Engine Integration

The library includes a complete Unreal Engine 5.6+ plugin example. See [`examples/unreal_plugin/README.md`](examples/unreal_plugin/README.md) for detailed integration instructions.

### Quick UE Integration

1. **Add as submodule** to your UE project:
   ```bash
   cd YourProject/Plugins
   git submodule add https://github.com/kahlertfr/trajectory-spatialhash.git
   ```

2. **Build libraries** using CI artifacts or locally:
   ```bash
   ./scripts/build-release-libs.sh
   ./scripts/package-thirdparty.sh
   ```

3. **Copy the plugin**:
   ```bash
   cp -r examples/unreal_plugin/TrajectorySpatialHash YourProject/Plugins/
   ```

4. **Use in C++ or Blueprints**:
   ```cpp
   #include "TSHUEBridge.h"
   
   FTSHGrid Grid = UTSHUEBridge::CreateGrid(10.0f);
   TArray<FString> Shards = {TEXT("C:/Data/shard.csv")};
   UTSHUEBridge::BuildFromShards(Grid, Shards);
   
   FTSHPoint Point;
   if (UTSHUEBridge::QueryNearest(Grid, FVector(0, 0, 0), Point)) {
       UE_LOG(LogTemp, Log, TEXT("Found point!"));
   }
   
   UTSHUEBridge::FreeGrid(Grid);
   ```

## Trajectory Data Format

The library supports two input formats:

### CSV Format (Simple)

Input CSV shard files should have the following format:

```csv
x,y,z,trajectory_id,point_index
0.0,0.0,0.0,1,0
1.0,2.0,3.0,1,1
5.0,6.0,7.0,2,0
...
```

- `x,y,z`: 3D coordinates of the point
- `trajectory_id`: Identifier for the trajectory this point belongs to
- `point_index`: Index of this point within its trajectory

### Binary Shard Format (Advanced)

The library also provides data structures to work with the binary Trajectory Data Shard format:

- **`shard-manifest.json`**: JSON metadata with shard parameters
- **`shard-meta.bin`**: Binary metadata (magic: "TDSH", 76 bytes)
- **`shard-trajmeta.bin`**: Per-trajectory metadata (40 bytes per trajectory)
- **`shard-data.bin`**: Time-series position data (magic: "TDDB")

This format stores trajectories as time-series data with fixed-size entries, enabling:
- Fast mmap-based access
- Efficient time-stepped trajectory queries
- Compact binary storage with NaN sentinels for invalid positions

See `include/trajectory_spatialhash/shard_format.h` for complete binary structure definitions.

## Architecture

### Components

- **Library (`libtrajectory_spatialhash`)**: Core spatial hash implementation with C++ and C APIs
- **CLI Tool (`tsh`)**: Command-line interface for building grids
- **Unreal Plugin**: Example UE integration with Blueprint support
- **Tests**: Comprehensive unit tests with Catch2

### Design Principles

- **PIMPL pattern**: Hide implementation details for stable ABI
- **No STL in UE headers**: C wrapper uses POD types only
- **Exception safety**: C wrapper catches all exceptions
- **Modern C++17**: Using standard library features appropriately
- **Cross-platform**: Consistent behavior on Windows, macOS, and Linux

## CI/CD

The project uses GitHub Actions for continuous integration:

- **Multi-platform builds**: Windows (MSVC), macOS (Clang), Linux (GCC)
- **Automated testing**: Runs test suite on all platforms
- **Artifact uploads**: Prebuilt libraries for each platform
- **ThirdParty packaging**: Ready-to-use UE plugin artifacts

Download prebuilt libraries from the [Actions tab](https://github.com/kahlertfr/trajectory-spatialhash/actions) after each successful build.

## Building for Specific Platforms

### Windows (Visual Studio 2022)

```cmd
mkdir build-win64
cd build-win64
cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
```

### macOS

```bash
mkdir build-mac
cd build-mac
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build .
```

### Linux

```bash
mkdir build-linux
cd build-linux
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
cmake --build .
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries instead of static |
| `BUILD_TESTING` | `ON` | Build unit tests |
| `BUILD_EXAMPLES` | `ON` | Build example programs |
| `BUILD_CLI` | `ON` | Build command-line tool |

## Development

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Adding New Features

1. Update the C++ API in `include/trajectory_spatialhash/spatial_hash.h`
2. Implement in `src/spatial_hash.cpp` using PIMPL
3. Update C wrapper in `include/trajectory_spatialhash/ue_wrapper.h` and `src/ue_wrapper.cpp`
4. Add tests in `tests/test_spatial_hash.cpp`
5. Update UE bridge in `examples/unreal_plugin/TrajectorySpatialHash/`

## Performance

The spatial hash grid provides:
- **O(1) average case** for nearest neighbor queries (with appropriate cell size)
- **O(k) for radius queries** where k is the number of cells intersecting the search radius
- **Memory efficient**: Only stores points and grid indices

Optimal cell size depends on your data distribution. Generally:
- Cell size ≈ average query radius works well
- Too small: More cells, slower grid traversal
- Too large: More points per cell, slower point checks

## License

MIT License - see [LICENSE](LICENSE) for details.

Copyright (c) 2025 Franziska Kahlert

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch
3. Add tests for new functionality
4. Ensure all tests pass
5. Submit a pull request

## Support

- 📖 Documentation: See README files in subdirectories
- 🐛 Issues: [GitHub Issues](https://github.com/kahlertfr/trajectory-spatialhash/issues)
- 💬 Discussions: [GitHub Discussions](https://github.com/kahlertfr/trajectory-spatialhash/discussions)
