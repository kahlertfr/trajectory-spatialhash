#include "trajectory_spatialhash/spatial_hash.h"
#include "trajectory_spatialhash/ue_wrapper.h"
#include <iostream>

using namespace trajectory_spatialhash;

int main() {
    std::cout << "=== Trajectory Spatial Hash Example ===\n\n";
    
    // Example 1: Using the C++ API
    std::cout << "Example 1: C++ API\n";
    {
        SpatialHashGrid grid(10.0);
        
        // Note: In a real application, you would have actual shard files
        // const char* shards[] = {"trajectory_shard_0.csv", "trajectory_shard_1.csv"};
        // grid.BuildFromShards(shards, 2);
        
        std::cout << "  Grid created with cell size: 10.0\n";
        std::cout << "  (In real usage, load shards with BuildFromShards)\n";
    }
    
    // Example 2: Using the C API (for Unreal Engine)
    std::cout << "\nExample 2: C API (UE-safe)\n";
    {
        TSH_Handle handle = TSH_Create(10.0);
        if (handle) {
            std::cout << "  Grid handle created successfully\n";
            
            // Query example (would work after loading data)
            // TSH_Point point;
            // if (TSH_QueryNearest(handle, 0.0, 0.0, 0.0, &point)) {
            //     std::cout << "  Found nearest point at: (" 
            //               << point.x << ", " << point.y << ", " << point.z << ")\n";
            // }
            
            TSH_Free(handle);
            std::cout << "  Grid handle freed\n";
        }
    }
    
    std::cout << "\n=== Example complete ===\n";
    return 0;
}
