#ifndef TRAJECTORY_SPATIALHASH_SPATIAL_HASH_H
#define TRAJECTORY_SPATIALHASH_SPATIAL_HASH_H

#include "config.h"
#include <cstddef>
#include <memory>

namespace trajectory_spatialhash {

// Forward declaration of implementation
class SpatialHashGridImpl;

// Point structure for query results
struct Point3D {
    double x, y, z;
    int trajectory_id;
    int point_index;
};

// Result structure for queries
struct QueryResult {
    Point3D* points;
    size_t count;
};

/**
 * @brief Spatial hash grid for fast nearest neighbor queries on trajectory data.
 * 
 * Uses PIMPL pattern to hide implementation details and maintain stable ABI.
 * Thread-safe for concurrent queries after building.
 */
class TSH_API SpatialHashGrid {
public:
    /**
     * @brief Construct a new Spatial Hash Grid object
     * 
     * @param cell_size Size of each cell in the spatial hash grid (in same units as trajectory data)
     */
    explicit SpatialHashGrid(double cell_size = 10.0);
    
    /**
     * @brief Destroy the Spatial Hash Grid object
     */
    ~SpatialHashGrid();
    
    // Disable copy, allow move
    SpatialHashGrid(const SpatialHashGrid&) = delete;
    SpatialHashGrid& operator=(const SpatialHashGrid&) = delete;
    SpatialHashGrid(SpatialHashGrid&&) noexcept;
    SpatialHashGrid& operator=(SpatialHashGrid&&) noexcept;
    
    /**
     * @brief Build spatial hash grid from trajectory shard files
     * 
     * @param shard_paths Array of file paths to trajectory shard CSV files
     * @param num_shards Number of shard files
     * @return true if successful, false on error
     */
    bool BuildFromShards(const char* const* shard_paths, size_t num_shards);
    
    /**
     * @brief Serialize the spatial hash grid to a binary file
     * 
     * @param output_path Path to output file
     * @return true if successful, false on error
     */
    bool Serialize(const char* output_path) const;
    
    /**
     * @brief Deserialize a spatial hash grid from a binary file
     * 
     * @param input_path Path to input file
     * @return true if successful, false on error
     */
    bool Deserialize(const char* input_path);
    
    /**
     * @brief Find the nearest point to the query location
     * 
     * @param x Query x coordinate
     * @param y Query y coordinate
     * @param z Query z coordinate
     * @param out_point Output point (caller must provide)
     * @return true if a point was found, false otherwise
     */
    bool Nearest(double x, double y, double z, Point3D& out_point) const;
    
    /**
     * @brief Query all points within a radius of the query location
     * 
     * @param x Query x coordinate
     * @param y Query y coordinate
     * @param z Query z coordinate
     * @param radius Search radius
     * @param out_result Output result structure (caller must free with FreeQueryResult)
     * @return true if successful, false on error
     */
    bool RadiusQuery(double x, double y, double z, double radius, QueryResult& out_result) const;
    
    /**
     * @brief Free memory allocated by RadiusQuery
     * 
     * @param result Result structure to free
     */
    static void FreeQueryResult(QueryResult& result);
    
    /**
     * @brief Get the number of points in the grid
     * 
     * @return size_t Number of points
     */
    size_t GetPointCount() const;
    
    /**
     * @brief Get the last error message
     * 
     * @return const char* Error message or nullptr if no error
     */
    const char* GetLastError() const;

private:
    std::unique_ptr<SpatialHashGridImpl> pimpl_;
};

} // namespace trajectory_spatialhash

#endif // TRAJECTORY_SPATIALHASH_SPATIAL_HASH_H
