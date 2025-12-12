#ifndef TRAJECTORY_SPATIALHASH_SHARD_FORMAT_H
#define TRAJECTORY_SPATIALHASH_SHARD_FORMAT_H

#include "config.h"
#include <cstdint>

namespace trajectory_spatialhash {

/**
 * @brief Binary format structures for Trajectory Data Shard specification
 * 
 * This header defines the on-disk layout for the binary trajectory data format.
 * All structs are packed (no padding) and use little-endian byte order.
 * 
 * File types:
 * - shard-manifest.json: JSON metadata (human-readable)
 * - shard-meta.bin: Binary metadata (magic: "TDSH")
 * - shard-trajmeta.bin: Per-trajectory metadata
 * - shard-data.bin: Time-series position data (magic: "TDDB")
 */

#pragma pack(push, 1)

/**
 * @brief Shard metadata - global parameters for the shard
 * 
 * Magic: "TDSH"
 * Total size: 76 bytes
 * Endianness: little-endian
 */
struct ShardMeta {
    char magic[4];                    // "TDSH"
    uint8_t format_version;           // 1
    uint8_t endianness_flag;          // 0 = little, 1 = big
    uint16_t reserved;                // padding
    double time_interval_seconds;     // Time interval in seconds
    int32_t time_step_interval_size;  // Number of time steps per interval
    int32_t entry_size_bytes;         // Bytes per trajectory entry in data files
    float bbox_min[3];                // Bounding box minimum (x, y, z)
    float bbox_max[3];                // Bounding box maximum (x, y, z)
    uint64_t trajectory_count;        // Total number of trajectories
    uint64_t first_trajectory_id;     // First trajectory ID in shard
    uint64_t last_trajectory_id;      // Last trajectory ID in shard
    uint32_t reserved2;               // Reserved for future use
};
static_assert(sizeof(ShardMeta) == 76, "ShardMeta must be 76 bytes");

/**
 * @brief Per-trajectory metadata
 * 
 * Total size: 40 bytes
 * One record per trajectory, sorted by trajectory_id
 */
struct TrajectoryMeta {
    uint64_t trajectory_id;           // Unique trajectory identifier
    int32_t start_time_step;          // First time step with valid data
    int32_t end_time_step;            // Last time step with valid data
    float extent[3];                  // Object half-extent in meters (x, y, z)
    uint32_t data_file_index;         // Which data file contains this trajectory
    uint64_t entry_offset_index;      // Entry index within data file for direct seek
};
static_assert(sizeof(TrajectoryMeta) == 40, "TrajectoryMeta must be 40 bytes");

/**
 * @brief Data block header for shard-data.bin files
 * 
 * Magic: "TDDB" (Trajectory Data Block)
 * Total size: 32 bytes
 * Endianness: little-endian
 */
struct DataBlockHeader {
    char magic[4];                    // "TDDB"
    uint8_t format_version;           // 1
    uint8_t endianness_flag;          // 0 = little, 1 = big
    uint16_t reserved;                // padding
    int32_t global_interval_index;    // Which time interval this file represents
    int32_t time_step_interval_size;  // Must match shard-meta
    int32_t trajectory_entry_count;   // Number of trajectory entries in this file
    int64_t data_section_offset;      // Byte offset where entries begin (normally 32)
    uint32_t reserved2;               // Reserved for future use
};
static_assert(sizeof(DataBlockHeader) == 32, "DataBlockHeader must be 32 bytes");

/**
 * @brief Trajectory entry header (variable size follows)
 * 
 * Fixed part: 16 bytes
 * Followed by: time_step_interval_size * 3 * sizeof(float) bytes of position data
 * 
 * Total entry size = entry_size_bytes (from ShardMeta)
 * Positions are stored as float[time_step_interval_size][3] (x, y, z per time step)
 * Invalid positions are represented by NaN values
 */
struct TrajectoryEntryHeader {
    uint64_t trajectory_id;                 // Trajectory identifier
    int32_t start_time_step_in_interval;    // First valid time step (0..interval_size-1), -1 if none
    int32_t valid_sample_count;             // Number of valid position samples
    // Followed by: float positions[time_step_interval_size][3]
};
static_assert(sizeof(TrajectoryEntryHeader) == 16, "TrajectoryEntryHeader must be 16 bytes");

#pragma pack(pop)

/**
 * @brief Helper constants for the binary format
 */
namespace ShardConstants {
    constexpr const char* SHARD_META_MAGIC = "TDSH";
    constexpr const char* DATA_BLOCK_MAGIC = "TDDB";
    constexpr uint8_t FORMAT_VERSION = 1;
    constexpr uint8_t ENDIAN_LITTLE = 0;
    constexpr uint8_t ENDIAN_BIG = 1;
}

/**
 * @brief Manifest field names for JSON parsing
 */
namespace ManifestFields {
    constexpr const char* SHARD_NAME = "shard_name";
    constexpr const char* FORMAT_VERSION = "format_version";
    constexpr const char* ENDIANNESS = "endianness";
    constexpr const char* COORDINATE_UNITS = "coordinate_units";
    constexpr const char* FLOAT_PRECISION = "float_precision";
    constexpr const char* TIME_UNITS = "time_units";
    constexpr const char* TIME_STEP_INTERVAL_SIZE = "time_step_interval_size";
    constexpr const char* TIME_INTERVAL_SECONDS = "time_interval_seconds";
    constexpr const char* ENTRY_SIZE_BYTES = "entry_size_bytes";
    constexpr const char* BOUNDING_BOX = "bounding_box";
    constexpr const char* TRAJECTORY_COUNT = "trajectory_count";
    constexpr const char* FIRST_TRAJECTORY_ID = "first_trajectory_id";
    constexpr const char* LAST_TRAJECTORY_ID = "last_trajectory_id";
    constexpr const char* CREATED_AT = "created_at";
    constexpr const char* CONVERTER_VERSION = "converter_version";
}

} // namespace trajectory_spatialhash

#endif // TRAJECTORY_SPATIALHASH_SHARD_FORMAT_H
