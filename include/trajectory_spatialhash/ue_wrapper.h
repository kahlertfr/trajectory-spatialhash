#ifndef TRAJECTORY_SPATIALHASH_UE_WRAPPER_H
#define TRAJECTORY_SPATIALHASH_UE_WRAPPER_H

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle to spatial hash grid
typedef void* TSH_Handle;

// POD structure for points (safe to pass across C boundary)
typedef struct {
    double x, y, z;
    int trajectory_id;
    int point_index;
} TSH_Point;

// POD structure for query results
typedef struct {
    TSH_Point* points;
    unsigned int count;
} TSH_QueryResult;

/**
 * @brief Create a new spatial hash grid
 * 
 * @param cell_size Size of each cell in the spatial hash grid
 * @return TSH_Handle Handle to the grid, or NULL on error
 */
TSH_API TSH_Handle TSH_Create(double cell_size);

/**
 * @brief Destroy a spatial hash grid and free resources
 * 
 * @param handle Handle to the grid
 */
TSH_API void TSH_Free(TSH_Handle handle);

/**
 * @brief Build spatial hash grid from trajectory shard files
 * 
 * @param handle Handle to the grid
 * @param shard_paths Array of file paths to trajectory shard CSV files
 * @param num_shards Number of shard files
 * @return 1 if successful, 0 on error
 */
TSH_API int TSH_BuildFromShards(TSH_Handle handle, const char* const* shard_paths, unsigned int num_shards);

/**
 * @brief Serialize the spatial hash grid to a binary file
 * 
 * @param handle Handle to the grid
 * @param output_path Path to output file
 * @return 1 if successful, 0 on error
 */
TSH_API int TSH_Serialize(TSH_Handle handle, const char* output_path);

/**
 * @brief Load a spatial hash grid from a binary file
 * 
 * @param handle Handle to the grid
 * @param input_path Path to input file
 * @return 1 if successful, 0 on error
 */
TSH_API int TSH_LoadGrid(TSH_Handle handle, const char* input_path);

/**
 * @brief Find the nearest point to the query location
 * 
 * @param handle Handle to the grid
 * @param x Query x coordinate
 * @param y Query y coordinate
 * @param z Query z coordinate
 * @param out_point Output point (caller must provide)
 * @return 1 if a point was found, 0 otherwise
 */
TSH_API int TSH_QueryNearest(TSH_Handle handle, double x, double y, double z, TSH_Point* out_point);

/**
 * @brief Query all points within a radius of the query location
 * 
 * @param handle Handle to the grid
 * @param x Query x coordinate
 * @param y Query y coordinate
 * @param z Query z coordinate
 * @param radius Search radius
 * @param out_result Output result structure (caller must free with TSH_FreeQueryResult)
 * @return 1 if successful, 0 on error
 */
TSH_API int TSH_RadiusQuery(TSH_Handle handle, double x, double y, double z, double radius, TSH_QueryResult* out_result);

/**
 * @brief Free memory allocated by TSH_RadiusQuery
 * 
 * @param result Result structure to free
 */
TSH_API void TSH_FreeQueryResult(TSH_QueryResult* result);

/**
 * @brief Get the number of points in the grid
 * 
 * @param handle Handle to the grid
 * @return Number of points, or 0 on error
 */
TSH_API unsigned int TSH_GetPointCount(TSH_Handle handle);

/**
 * @brief Get the last error message
 * 
 * @param handle Handle to the grid
 * @return Error message or NULL if no error
 */
TSH_API const char* TSH_GetLastError(TSH_Handle handle);

#ifdef __cplusplus
}
#endif

#endif // TRAJECTORY_SPATIALHASH_UE_WRAPPER_H
