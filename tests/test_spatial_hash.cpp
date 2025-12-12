#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "trajectory_spatialhash/spatial_hash.h"
#include "trajectory_spatialhash/ue_wrapper.h"
#include <fstream>
#include <cstdio>
#include <cmath>

using namespace trajectory_spatialhash;
using Catch::Approx;

// Helper to create a test CSV file
void CreateTestShard(const char* path) {
    std::ofstream out(path);
    out << "x,y,z,trajectory_id,point_index\n";
    // Create a simple grid of points
    out << "0.0,0.0,0.0,1,0\n";
    out << "10.0,0.0,0.0,1,1\n";
    out << "20.0,0.0,0.0,1,2\n";
    out << "0.0,10.0,0.0,2,0\n";
    out << "10.0,10.0,0.0,2,1\n";
    out << "20.0,10.0,0.0,2,2\n";
}

TEST_CASE("SpatialHashGrid basic operations", "[spatial_hash]") {
    const char* test_file = "/tmp/test_shard.csv";
    CreateTestShard(test_file);
    
    SECTION("Build from shards") {
        SpatialHashGrid grid(5.0);
        const char* shards[] = {test_file};
        
        REQUIRE(grid.BuildFromShards(shards, 1));
        REQUIRE(grid.GetPointCount() == 6);
    }
    
    SECTION("Nearest neighbor query") {
        SpatialHashGrid grid(5.0);
        const char* shards[] = {test_file};
        grid.BuildFromShards(shards, 1);
        
        Point3D result;
        REQUIRE(grid.Nearest(1.0, 1.0, 0.0, result));
        REQUIRE(result.x == Approx(0.0).epsilon(0.01));
        REQUIRE(result.y == Approx(0.0).epsilon(0.01));
        REQUIRE(result.trajectory_id == 1);
    }
    
    SECTION("Radius query") {
        SpatialHashGrid grid(5.0);
        const char* shards[] = {test_file};
        grid.BuildFromShards(shards, 1);
        
        QueryResult result;
        REQUIRE(grid.RadiusQuery(10.0, 5.0, 0.0, 7.0, result));
        REQUIRE(result.count >= 2); // Should find at least points at (10,0) and (10,10)
        
        SpatialHashGrid::FreeQueryResult(result);
    }
    
    SECTION("Serialize and deserialize") {
        const char* grid_file = "/tmp/test_grid.bin";
        
        {
            SpatialHashGrid grid(5.0);
            const char* shards[] = {test_file};
            grid.BuildFromShards(shards, 1);
            REQUIRE(grid.Serialize(grid_file));
        }
        
        {
            SpatialHashGrid grid(1.0); // Different cell size
            REQUIRE(grid.Deserialize(grid_file));
            REQUIRE(grid.GetPointCount() == 6);
            
            Point3D result;
            REQUIRE(grid.Nearest(1.0, 1.0, 0.0, result));
        }
        
        std::remove(grid_file);
    }
    
    std::remove(test_file);
}

TEST_CASE("C wrapper API", "[ue_wrapper]") {
    const char* test_file = "/tmp/test_shard_c.csv";
    CreateTestShard(test_file);
    
    SECTION("Basic C API operations") {
        TSH_Handle handle = TSH_Create(5.0);
        REQUIRE(handle != nullptr);
        
        const char* shards[] = {test_file};
        REQUIRE(TSH_BuildFromShards(handle, shards, 1) == 1);
        REQUIRE(TSH_GetPointCount(handle) == 6);
        
        TSH_Point point;
        REQUIRE(TSH_QueryNearest(handle, 1.0, 1.0, 0.0, &point) == 1);
        REQUIRE(point.x == Approx(0.0).epsilon(0.01));
        
        TSH_QueryResult result;
        REQUIRE(TSH_RadiusQuery(handle, 10.0, 5.0, 0.0, 7.0, &result) == 1);
        REQUIRE(result.count >= 2);
        TSH_FreeQueryResult(&result);
        
        TSH_Free(handle);
    }
    
    SECTION("C API serialization") {
        const char* grid_file = "/tmp/test_grid_c.bin";
        
        TSH_Handle handle1 = TSH_Create(5.0);
        const char* shards[] = {test_file};
        TSH_BuildFromShards(handle1, shards, 1);
        REQUIRE(TSH_Serialize(handle1, grid_file) == 1);
        TSH_Free(handle1);
        
        TSH_Handle handle2 = TSH_Create(1.0);
        REQUIRE(TSH_LoadGrid(handle2, grid_file) == 1);
        REQUIRE(TSH_GetPointCount(handle2) == 6);
        TSH_Free(handle2);
        
        std::remove(grid_file);
    }
    
    std::remove(test_file);
}

TEST_CASE("Edge cases", "[spatial_hash]") {
    SECTION("Empty grid operations") {
        SpatialHashGrid grid(5.0);
        
        Point3D result;
        REQUIRE_FALSE(grid.Nearest(0.0, 0.0, 0.0, result));
        
        QueryResult qresult;
        REQUIRE(grid.RadiusQuery(0.0, 0.0, 0.0, 10.0, qresult));
        REQUIRE(qresult.count == 0);
        REQUIRE(qresult.points == nullptr);
    }
    
    SECTION("Invalid file paths") {
        SpatialHashGrid grid(5.0);
        const char* invalid[] = {"/nonexistent/path.csv"};
        REQUIRE_FALSE(grid.BuildFromShards(invalid, 1));
        REQUIRE(grid.GetLastError() != nullptr);
    }
}
