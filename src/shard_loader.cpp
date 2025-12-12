#include "trajectory_spatialhash/shard_format.h"
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <cmath>

namespace trajectory_spatialhash {

/**
 * @brief Helper functions for reading binary shard format
 * 
 * These utilities provide basic functionality to read and validate
 * the binary trajectory data shard format according to the specification.
 */
namespace ShardReader {

/**
 * @brief Validate magic number in binary file
 */
bool ValidateMagic(const char* magic, const char* expected, size_t len = 4) {
    return std::memcmp(magic, expected, len) == 0;
}

/**
 * @brief Check if the system is little-endian
 */
bool IsLittleEndian() {
    uint32_t test = 1;
    return *reinterpret_cast<uint8_t*>(&test) == 1;
}

/**
 * @brief Read shard metadata from binary file
 * 
 * @param path Path to shard-meta.bin file
 * @param meta Output structure to fill
 * @return true if successful, false on error
 */
bool ReadShardMeta(const char* path, ShardMeta& meta) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&meta), sizeof(ShardMeta));
    if (!file) {
        return false;
    }
    
    // Validate magic number
    if (!ValidateMagic(meta.magic, ShardConstants::SHARD_META_MAGIC)) {
        return false;
    }
    
    // Check format version
    if (meta.format_version != ShardConstants::FORMAT_VERSION) {
        return false;
    }
    
    // Check endianness matches system
    bool is_little = IsLittleEndian();
    if ((meta.endianness_flag == ShardConstants::ENDIAN_LITTLE) != is_little) {
        // Endianness mismatch - would need byte swapping
        return false;
    }
    
    return true;
}

/**
 * @brief Read trajectory metadata from binary file
 * 
 * @param path Path to shard-trajmeta.bin file
 * @param metas Output vector to fill with trajectory metadata
 * @param expected_count Expected number of trajectories (from shard-meta)
 * @return true if successful, false on error
 */
bool ReadTrajectoryMetas(const char* path, std::vector<TrajectoryMeta>& metas, uint64_t expected_count) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // Get file size to validate
    file.seekg(0, std::ios::end);
    std::streampos file_size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    size_t expected_size = expected_count * sizeof(TrajectoryMeta);
    if (static_cast<size_t>(file_size) != expected_size) {
        return false;
    }
    
    // Read all trajectory metadata records
    metas.resize(expected_count);
    file.read(reinterpret_cast<char*>(metas.data()), expected_size);
    
    if (!file) {
        return false;
    }
    
    // Validate that trajectory IDs are sorted (specification requirement)
    for (size_t i = 1; i < metas.size(); ++i) {
        if (metas[i].trajectory_id <= metas[i-1].trajectory_id) {
            return false; // Not sorted or has duplicates
        }
    }
    
    return true;
}

/**
 * @brief Read data block header from binary file
 * 
 * @param path Path to shard-data.bin file
 * @param header Output structure to fill
 * @return true if successful, false on error
 */
bool ReadDataBlockHeader(const char* path, DataBlockHeader& header) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.read(reinterpret_cast<char*>(&header), sizeof(DataBlockHeader));
    if (!file) {
        return false;
    }
    
    // Validate magic number
    if (!ValidateMagic(header.magic, ShardConstants::DATA_BLOCK_MAGIC)) {
        return false;
    }
    
    // Check format version
    if (header.format_version != ShardConstants::FORMAT_VERSION) {
        return false;
    }
    
    // Check endianness matches system
    bool is_little = IsLittleEndian();
    if ((header.endianness_flag == ShardConstants::ENDIAN_LITTLE) != is_little) {
        return false;
    }
    
    return true;
}

/**
 * @brief Read trajectory entry header and position data
 * 
 * @param file Input file stream positioned at entry start
 * @param entry_header Output structure for entry header
 * @param positions Output vector for position data (x,y,z per time step)
 * @param time_step_interval_size Number of time steps per interval
 * @return true if successful, false on error
 */
bool ReadTrajectoryEntry(std::ifstream& file, TrajectoryEntryHeader& entry_header, 
                        std::vector<float>& positions, int32_t time_step_interval_size) {
    // Read entry header
    file.read(reinterpret_cast<char*>(&entry_header), sizeof(TrajectoryEntryHeader));
    if (!file) {
        return false;
    }
    
    // Read position data: time_step_interval_size * 3 floats (x, y, z per step)
    size_t position_count = time_step_interval_size * 3;
    positions.resize(position_count);
    file.read(reinterpret_cast<char*>(positions.data()), position_count * sizeof(float));
    
    if (!file) {
        return false;
    }
    
    return true;
}

/**
 * @brief Check if a position is valid (not NaN)
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return true if position is valid (no NaN values)
 */
bool IsValidPosition(float x, float y, float z) {
    return !std::isnan(x) && !std::isnan(y) && !std::isnan(z);
}

/**
 * @brief Extract valid positions from trajectory entry data
 * 
 * @param positions Raw position data (x,y,z per time step)
 * @param time_step_interval_size Number of time steps
 * @param valid_positions Output vector of valid (non-NaN) positions
 * @return Number of valid positions found
 */
size_t ExtractValidPositions(const std::vector<float>& positions, 
                            int32_t time_step_interval_size,
                            std::vector<std::tuple<float, float, float>>& valid_positions) {
    valid_positions.clear();
    
    for (int32_t i = 0; i < time_step_interval_size; ++i) {
        size_t offset = i * 3;
        float x = positions[offset];
        float y = positions[offset + 1];
        float z = positions[offset + 2];
        
        if (IsValidPosition(x, y, z)) {
            valid_positions.emplace_back(x, y, z);
        }
    }
    
    return valid_positions.size();
}

} // namespace ShardReader

} // namespace trajectory_spatialhash
