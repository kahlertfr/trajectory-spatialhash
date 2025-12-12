#include "trajectory_spatialhash/spatial_hash.h"
#include <vector>
#include <unordered_map>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace trajectory_spatialhash {

// Hash function for 3D grid cells
struct CellHash {
    size_t operator()(const std::tuple<int, int, int>& cell) const {
        auto [x, y, z] = cell;
        // Simple hash combination
        size_t h1 = std::hash<int>{}(x);
        size_t h2 = std::hash<int>{}(y);
        size_t h3 = std::hash<int>{}(z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

// Internal implementation
class SpatialHashGridImpl {
public:
    explicit SpatialHashGridImpl(double cell_size) 
        : cell_size_(cell_size), inv_cell_size_(1.0 / cell_size) {}
    
    bool BuildFromShards(const char* const* shard_paths, size_t num_shards) {
        points_.clear();
        grid_.clear();
        last_error_.clear();
        
        for (size_t i = 0; i < num_shards; ++i) {
            if (!LoadShard(shard_paths[i])) {
                return false;
            }
        }
        
        // Build spatial hash grid
        for (size_t i = 0; i < points_.size(); ++i) {
            const auto& pt = points_[i];
            auto cell = GetCell(pt.x, pt.y, pt.z);
            grid_[cell].push_back(i);
        }
        
        return true;
    }
    
    bool Serialize(const char* output_path) const {
        std::ofstream out(output_path, std::ios::binary);
        if (!out) {
            const_cast<SpatialHashGridImpl*>(this)->last_error_ = "Failed to open output file";
            return false;
        }
        
        // Write header
        const char magic[4] = {'T', 'S', 'H', '1'};
        out.write(magic, 4);
        out.write(reinterpret_cast<const char*>(&cell_size_), sizeof(cell_size_));
        
        // Write points
        size_t num_points = points_.size();
        out.write(reinterpret_cast<const char*>(&num_points), sizeof(num_points));
        out.write(reinterpret_cast<const char*>(points_.data()), num_points * sizeof(Point3D));
        
        // Write grid
        size_t num_cells = grid_.size();
        out.write(reinterpret_cast<const char*>(&num_cells), sizeof(num_cells));
        
        for (const auto& [cell, indices] : grid_) {
            auto [x, y, z] = cell;
            out.write(reinterpret_cast<const char*>(&x), sizeof(x));
            out.write(reinterpret_cast<const char*>(&y), sizeof(y));
            out.write(reinterpret_cast<const char*>(&z), sizeof(z));
            
            size_t num_indices = indices.size();
            out.write(reinterpret_cast<const char*>(&num_indices), sizeof(num_indices));
            out.write(reinterpret_cast<const char*>(indices.data()), num_indices * sizeof(size_t));
        }
        
        return true;
    }
    
    bool Deserialize(const char* input_path) {
        std::ifstream in(input_path, std::ios::binary);
        if (!in) {
            last_error_ = "Failed to open input file";
            return false;
        }
        
        // Read header
        char magic[4];
        in.read(magic, 4);
        if (std::memcmp(magic, "TSH1", 4) != 0) {
            last_error_ = "Invalid file format";
            return false;
        }
        
        in.read(reinterpret_cast<char*>(&cell_size_), sizeof(cell_size_));
        inv_cell_size_ = 1.0 / cell_size_;
        
        // Read points
        size_t num_points;
        in.read(reinterpret_cast<char*>(&num_points), sizeof(num_points));
        points_.resize(num_points);
        in.read(reinterpret_cast<char*>(points_.data()), num_points * sizeof(Point3D));
        
        // Read grid
        size_t num_cells;
        in.read(reinterpret_cast<char*>(&num_cells), sizeof(num_cells));
        
        grid_.clear();
        for (size_t i = 0; i < num_cells; ++i) {
            int x, y, z;
            in.read(reinterpret_cast<char*>(&x), sizeof(x));
            in.read(reinterpret_cast<char*>(&y), sizeof(y));
            in.read(reinterpret_cast<char*>(&z), sizeof(z));
            
            size_t num_indices;
            in.read(reinterpret_cast<char*>(&num_indices), sizeof(num_indices));
            
            std::vector<size_t> indices(num_indices);
            in.read(reinterpret_cast<char*>(indices.data()), num_indices * sizeof(size_t));
            
            grid_[std::make_tuple(x, y, z)] = std::move(indices);
        }
        
        return true;
    }
    
    bool Nearest(double x, double y, double z, Point3D& out_point) const {
        if (points_.empty()) {
            return false;
        }
        
        auto center_cell = GetCell(x, y, z);
        
        // Search in expanding cube pattern
        for (int radius = 0; radius <= 10; ++radius) {
            double min_dist_sq = std::numeric_limits<double>::max();
            bool found = false;
            
            for (int dx = -radius; dx <= radius; ++dx) {
                for (int dy = -radius; dy <= radius; ++dy) {
                    for (int dz = -radius; dz <= radius; ++dz) {
                        // Only check cells on the surface of the cube
                        if (radius > 0 && std::abs(dx) != radius && std::abs(dy) != radius && std::abs(dz) != radius) {
                            continue;
                        }
                        
                        auto cell = std::make_tuple(
                            std::get<0>(center_cell) + dx,
                            std::get<1>(center_cell) + dy,
                            std::get<2>(center_cell) + dz
                        );
                        
                        auto it = grid_.find(cell);
                        if (it == grid_.end()) continue;
                        
                        for (size_t idx : it->second) {
                            const auto& pt = points_[idx];
                            double dx_val = pt.x - x;
                            double dy_val = pt.y - y;
                            double dz_val = pt.z - z;
                            double dist_sq = dx_val * dx_val + dy_val * dy_val + dz_val * dz_val;
                            
                            if (dist_sq < min_dist_sq) {
                                min_dist_sq = dist_sq;
                                out_point = pt;
                                found = true;
                            }
                        }
                    }
                }
            }
            
            if (found) {
                return true;
            }
        }
        
        return false;
    }
    
    bool RadiusQuery(double x, double y, double z, double radius, QueryResult& out_result) const {
        std::vector<Point3D> results;
        double radius_sq = radius * radius;
        
        // Determine cell range to check
        int cell_radius = static_cast<int>(std::ceil(radius * inv_cell_size_));
        auto center_cell = GetCell(x, y, z);
        
        for (int dx = -cell_radius; dx <= cell_radius; ++dx) {
            for (int dy = -cell_radius; dy <= cell_radius; ++dy) {
                for (int dz = -cell_radius; dz <= cell_radius; ++dz) {
                    auto cell = std::make_tuple(
                        std::get<0>(center_cell) + dx,
                        std::get<1>(center_cell) + dy,
                        std::get<2>(center_cell) + dz
                    );
                    
                    auto it = grid_.find(cell);
                    if (it == grid_.end()) continue;
                    
                    for (size_t idx : it->second) {
                        const auto& pt = points_[idx];
                        double dx_val = pt.x - x;
                        double dy_val = pt.y - y;
                        double dz_val = pt.z - z;
                        double dist_sq = dx_val * dx_val + dy_val * dy_val + dz_val * dz_val;
                        
                        if (dist_sq <= radius_sq) {
                            results.push_back(pt);
                        }
                    }
                }
            }
        }
        
        // Allocate result
        out_result.count = results.size();
        if (out_result.count > 0) {
            out_result.points = new Point3D[out_result.count];
            std::copy(results.begin(), results.end(), out_result.points);
        } else {
            out_result.points = nullptr;
        }
        
        return true;
    }
    
    size_t GetPointCount() const {
        return points_.size();
    }
    
    const char* GetLastError() const {
        return last_error_.empty() ? nullptr : last_error_.c_str();
    }

private:
    std::tuple<int, int, int> GetCell(double x, double y, double z) const {
        return std::make_tuple(
            static_cast<int>(std::floor(x * inv_cell_size_)),
            static_cast<int>(std::floor(y * inv_cell_size_)),
            static_cast<int>(std::floor(z * inv_cell_size_))
        );
    }
    
    bool LoadShard(const char* path) {
        std::ifstream file(path);
        if (!file) {
            last_error_ = std::string("Failed to open shard file: ") + path;
            return false;
        }
        
        std::string line;
        // Skip header if present
        if (std::getline(file, line)) {
            // Check if first line looks like header
            if (line.find("x") != std::string::npos || line.find("X") != std::string::npos) {
                // Skip header
            } else {
                // Parse as data
                file.seekg(0);
            }
        }
        
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            std::stringstream ss(line);
            Point3D pt;
            char comma;
            
            // Expected format: x,y,z,trajectory_id,point_index
            if (ss >> pt.x >> comma >> pt.y >> comma >> pt.z >> comma >> pt.trajectory_id >> comma >> pt.point_index) {
                points_.push_back(pt);
            }
        }
        
        return true;
    }
    
    double cell_size_;
    double inv_cell_size_;
    std::vector<Point3D> points_;
    std::unordered_map<std::tuple<int, int, int>, std::vector<size_t>, CellHash> grid_;
    std::string last_error_;
};

// SpatialHashGrid implementation
SpatialHashGrid::SpatialHashGrid(double cell_size)
    : pimpl_(std::make_unique<SpatialHashGridImpl>(cell_size)) {}

SpatialHashGrid::~SpatialHashGrid() = default;

SpatialHashGrid::SpatialHashGrid(SpatialHashGrid&&) noexcept = default;
SpatialHashGrid& SpatialHashGrid::operator=(SpatialHashGrid&&) noexcept = default;

bool SpatialHashGrid::BuildFromShards(const char* const* shard_paths, size_t num_shards) {
    return pimpl_->BuildFromShards(shard_paths, num_shards);
}

bool SpatialHashGrid::Serialize(const char* output_path) const {
    return pimpl_->Serialize(output_path);
}

bool SpatialHashGrid::Deserialize(const char* input_path) {
    return pimpl_->Deserialize(input_path);
}

bool SpatialHashGrid::Nearest(double x, double y, double z, Point3D& out_point) const {
    return pimpl_->Nearest(x, y, z, out_point);
}

bool SpatialHashGrid::RadiusQuery(double x, double y, double z, double radius, QueryResult& out_result) const {
    return pimpl_->RadiusQuery(x, y, z, radius, out_result);
}

void SpatialHashGrid::FreeQueryResult(QueryResult& result) {
    delete[] result.points;
    result.points = nullptr;
    result.count = 0;
}

size_t SpatialHashGrid::GetPointCount() const {
    return pimpl_->GetPointCount();
}

const char* SpatialHashGrid::GetLastError() const {
    return pimpl_->GetLastError();
}

} // namespace trajectory_spatialhash
