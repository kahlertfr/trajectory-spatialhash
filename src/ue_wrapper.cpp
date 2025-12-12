#include "trajectory_spatialhash/ue_wrapper.h"
#include "trajectory_spatialhash/spatial_hash.h"
#include <cstring>

using namespace trajectory_spatialhash;

// Convert C++ Point3D to C TSH_Point
static void ConvertPoint(const Point3D& src, TSH_Point* dst) {
    dst->x = src.x;
    dst->y = src.y;
    dst->z = src.z;
    dst->trajectory_id = src.trajectory_id;
    dst->point_index = src.point_index;
}

TSH_Handle TSH_Create(double cell_size) {
    try {
        return new SpatialHashGrid(cell_size);
    } catch (...) {
        return nullptr;
    }
}

void TSH_Free(TSH_Handle handle) {
    if (handle) {
        delete static_cast<SpatialHashGrid*>(handle);
    }
}

int TSH_BuildFromShards(TSH_Handle handle, const char* const* shard_paths, unsigned int num_shards) {
    if (!handle) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        return grid->BuildFromShards(shard_paths, num_shards) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int TSH_Serialize(TSH_Handle handle, const char* output_path) {
    if (!handle) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        return grid->Serialize(output_path) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int TSH_LoadGrid(TSH_Handle handle, const char* input_path) {
    if (!handle) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        return grid->Deserialize(input_path) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

int TSH_QueryNearest(TSH_Handle handle, double x, double y, double z, TSH_Point* out_point) {
    if (!handle || !out_point) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        Point3D pt;
        if (grid->Nearest(x, y, z, pt)) {
            ConvertPoint(pt, out_point);
            return 1;
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

int TSH_RadiusQuery(TSH_Handle handle, double x, double y, double z, double radius, TSH_QueryResult* out_result) {
    if (!handle || !out_result) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        QueryResult result;
        if (grid->RadiusQuery(x, y, z, radius, result)) {
            out_result->count = static_cast<unsigned int>(result.count);
            if (result.count > 0) {
                out_result->points = new TSH_Point[result.count];
                for (size_t i = 0; i < result.count; ++i) {
                    ConvertPoint(result.points[i], &out_result->points[i]);
                }
                SpatialHashGrid::FreeQueryResult(result);
            } else {
                out_result->points = nullptr;
            }
            return 1;
        }
        return 0;
    } catch (...) {
        return 0;
    }
}

void TSH_FreeQueryResult(TSH_QueryResult* result) {
    if (result && result->points) {
        delete[] result->points;
        result->points = nullptr;
        result->count = 0;
    }
}

unsigned int TSH_GetPointCount(TSH_Handle handle) {
    if (!handle) return 0;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        return static_cast<unsigned int>(grid->GetPointCount());
    } catch (...) {
        return 0;
    }
}

const char* TSH_GetLastError(TSH_Handle handle) {
    if (!handle) return nullptr;
    
    try {
        auto* grid = static_cast<SpatialHashGrid*>(handle);
        return grid->GetLastError();
    } catch (...) {
        return nullptr;
    }
}
