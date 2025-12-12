# Unreal Engine Integration

This folder contains an example Unreal Engine plugin that demonstrates how to integrate the trajectory_spatialhash library into UE5.6+ projects.

## Integration Approaches

### Approach 1: Prebuilt Static Libraries (Recommended for Production)

This approach uses prebuilt static libraries for each platform, which is the standard UE plugin distribution method.

#### Steps:

1. **Add as Git Submodule** (in your UE project's `Plugins/` directory):
   ```bash
   cd YourProject/Plugins
   git submodule add https://github.com/kahlertfr/trajectory-spatialhash.git
   ```

2. **Build the Libraries**:
   
   Run the CI workflow or build manually for each target platform:
   
   **Windows (Visual Studio 2022, x64):**
   ```cmd
   mkdir build-win64 && cd build-win64
   cmake .. -G "Visual Studio 17 2022" -A x64 -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF
   cmake --build . --config Release
   ```
   
   **macOS:**
   ```bash
   mkdir build-mac && cd build-mac
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF
   cmake --build .
   ```
   
   **Linux:**
   ```bash
   mkdir build-linux && cd build-linux
   cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DBUILD_EXAMPLES=OFF
   cmake --build .
   ```

3. **Copy Built Artifacts** to ThirdParty Layout:
   
   Use the provided script or manually:
   ```bash
   ./scripts/package-thirdparty.sh
   ```
   
   This creates:
   ```
   ThirdParty/trajectory_spatialhash/
   ├── include/
   │   └── trajectory_spatialhash/
   │       ├── config.h
   │       ├── spatial_hash.h
   │       └── ue_wrapper.h
   └── lib/
       ├── win64/trajectory_spatialhash.lib
       ├── mac/libtrajectory_spatialhash.a
       └── linux/libtrajectory_spatialhash.a
   ```

4. **Copy Plugin to Your Project**:
   ```bash
   cp -r examples/unreal_plugin/TrajectorySpatialHash YourProject/Plugins/
   ```

5. **Enable the Plugin** in your `.uproject` file or through the UE Editor plugins menu.

6. **Use the API** in your game code:
   ```cpp
   #include "TSHUEBridge.h"
   
   // Create grid
   FTSHGrid Grid = UTSHUEBridge::CreateGrid(10.0f);
   
   // Build from shards
   TArray<FString> ShardPaths = {TEXT("C:/Data/shard1.csv")};
   bool bSuccess = UTSHUEBridge::BuildFromShards(Grid, ShardPaths);
   
   // Query nearest
   FTSHPoint Point;
   if (UTSHUEBridge::QueryNearest(Grid, FVector(0, 0, 0), Point)) {
       UE_LOG(LogTemp, Log, TEXT("Found point at (%f, %f, %f)"), Point.X, Point.Y, Point.Z);
   }
   
   // Clean up
   UTSHUEBridge::FreeGrid(Grid);
   ```

### Approach 2: Source-in-Plugin (For Development/Debugging)

This approach compiles the library source code directly as part of the plugin, making debugging easier.

#### Steps:

1. Add the library sources to the plugin's `Source/` directory:
   ```
   TrajectorySpatialHash/
   └── Source/
       └── TrajectorySpatialHash/
           ├── Private/
           │   ├── spatial_hash.cpp
           │   ├── ue_wrapper.cpp
           │   └── TSHUEBridge.cpp
           └── Public/
               ├── config.h
               ├── spatial_hash.h
               ├── ue_wrapper.h
               └── TSHUEBridge.h
   ```

2. Update `TrajectorySpatialHash.Build.cs` to compile the sources directly (see comments in Build.cs).

3. **Trade-offs**:
   - ✅ Easier debugging (step through library code)
   - ✅ No separate build step needed
   - ✅ Single-platform development friendly
   - ❌ Slower iteration (recompiles with engine)
   - ❌ Exposes implementation details
   - ❌ Not typical for distributed plugins

## CI/CD Integration

The GitHub Actions workflow automatically builds libraries for all platforms and uploads them as artifacts. Download these artifacts and extract them to the `ThirdParty/` layout for easy distribution.

## Requirements

- Unreal Engine 5.6+
- C++17 support (enabled by default in UE 5.6)
- For prebuilt libs: CMake 3.16+, appropriate C++ compiler per platform

## Troubleshooting

### Linker Errors on Windows
- Ensure you're linking `trajectory_spatialhash.lib` (not `.dll.lib`)
- Verify platform is set to Win64 in Build.cs

### Missing Symbols
- Check that the library was built for the correct platform/architecture
- Ensure C++ exceptions are enabled in Build.cs if needed

### Runtime Crashes
- Verify binary compatibility (same compiler, same C++ runtime)
- Use the C API (`ue_wrapper.h`) to avoid ABI issues

## License

This plugin integration example and the trajectory_spatialhash library are both licensed under the MIT License.
