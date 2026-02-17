// Verification tool to check if binary format matches specification-spatial-hash-table.md
// This can be compiled as a standalone tool or integrated into test suite

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

// Expected format from specification-spatial-hash-table.md
struct SpecHeader {
    uint32_t Magic;          // Offset 0,  Size 4  - 0x54534854
    uint32_t Version;        // Offset 4,  Size 4  - 1
    uint32_t TimeStep;       // Offset 8,  Size 4
    float    CellSize;       // Offset 12, Size 4
    float    BBoxMinX;       // Offset 16, Size 4
    float    BBoxMinY;       // Offset 20, Size 4
    float    BBoxMinZ;       // Offset 24, Size 4
    float    BBoxMaxX;       // Offset 28, Size 4
    float    BBoxMaxY;       // Offset 32, Size 4
    float    BBoxMaxZ;       // Offset 36, Size 4
    uint32_t NumEntries;     // Offset 40, Size 4
    uint32_t NumTrajectoryIds; // Offset 44, Size 4
    uint32_t Reserved[4];    // Offset 48, Size 16
};

struct SpecEntry {
    uint64_t ZOrderKey;      // Offset 0,  Size 8
    uint32_t StartIndex;     // Offset 8,  Size 4
    uint32_t TrajectoryCount; // Offset 12, Size 4
};

static_assert(sizeof(SpecHeader) == 64, "Header must be 64 bytes");
static_assert(sizeof(SpecEntry) == 16, "Entry must be 16 bytes");

void verifyBinaryFormat(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        printf("ERROR: Cannot open file %s\n", filename);
        return;
    }

    // Read and verify header
    SpecHeader header;
    if (fread(&header, sizeof(SpecHeader), 1, file) != 1) {
        printf("ERROR: Failed to read header\n");
        fclose(file);
        return;
    }

    printf("=== Binary Format Verification ===\n");
    printf("File: %s\n\n", filename);

    printf("HEADER (64 bytes):\n");
    printf("  Offset 0:  Magic = 0x%08X (expected 0x54534854 = 'TSHT')\n", header.Magic);
    printf("  Offset 4:  Version = %u (expected 1)\n", header.Version);
    printf("  Offset 8:  TimeStep = %u\n", header.TimeStep);
    printf("  Offset 12: CellSize = %.3f\n", header.CellSize);
    printf("  Offset 16: BBoxMinX = %.3f\n", header.BBoxMinX);
    printf("  Offset 20: BBoxMinY = %.3f\n", header.BBoxMinY);
    printf("  Offset 24: BBoxMinZ = %.3f\n", header.BBoxMinZ);
    printf("  Offset 28: BBoxMaxX = %.3f\n", header.BBoxMaxX);
    printf("  Offset 32: BBoxMaxY = %.3f\n", header.BBoxMaxY);
    printf("  Offset 36: BBoxMaxZ = %.3f\n", header.BBoxMaxZ);
    printf("  Offset 40: NumEntries = %u\n", header.NumEntries);
    printf("  Offset 44: NumTrajectoryIds = %u\n", header.NumTrajectoryIds);
    printf("  Offset 48-63: Reserved (16 bytes)\n");

    // Verify magic number
    if (header.Magic != 0x54534854) {
        printf("\n❌ FAIL: Invalid magic number!\n");
    } else {
        printf("\n✓ Magic number correct\n");
    }

    // Verify version
    if (header.Version != 1) {
        printf("❌ FAIL: Invalid version!\n");
    } else {
        printf("✓ Version correct\n");
    }

    // Read and verify entries
    printf("\nENTRIES (%u entries, 16 bytes each):\n", header.NumEntries);
    
    std::vector<SpecEntry> entries(header.NumEntries);
    if (header.NumEntries > 0) {
        if (fread(entries.data(), sizeof(SpecEntry), header.NumEntries, file) != header.NumEntries) {
            printf("ERROR: Failed to read entries\n");
            fclose(file);
            return;
        }

        // Show first few entries
        uint32_t showCount = header.NumEntries < 5 ? header.NumEntries : 5;
        for (uint32_t i = 0; i < showCount; i++) {
            printf("  Entry[%u]: ZOrderKey=0x%016llX, StartIndex=%u, TrajectoryCount=%u\n",
                   i, (unsigned long long)entries[i].ZOrderKey, 
                   entries[i].StartIndex, entries[i].TrajectoryCount);
        }
        if (header.NumEntries > 5) {
            printf("  ... (%u more entries)\n", header.NumEntries - 5);
        }

        // Verify entries are sorted
        bool sorted = true;
        for (uint32_t i = 1; i < header.NumEntries; i++) {
            if (entries[i].ZOrderKey <= entries[i-1].ZOrderKey) {
                printf("\n❌ FAIL: Entries not sorted at index %u!\n", i);
                sorted = false;
                break;
            }
        }
        if (sorted) {
            printf("\n✓ Entries are sorted by Z-Order key\n");
        }
    }

    // Read and verify trajectory IDs
    printf("\nTRAJECTORY IDs (%u IDs, 4 bytes each):\n", header.NumTrajectoryIds);
    
    std::vector<uint32_t> trajectoryIds(header.NumTrajectoryIds);
    if (header.NumTrajectoryIds > 0) {
        if (fread(trajectoryIds.data(), sizeof(uint32_t), header.NumTrajectoryIds, file) != header.NumTrajectoryIds) {
            printf("ERROR: Failed to read trajectory IDs\n");
            fclose(file);
            return;
        }

        // Show first few IDs
        uint32_t showCount = header.NumTrajectoryIds < 10 ? header.NumTrajectoryIds : 10;
        printf("  First %u IDs: [", showCount);
        for (uint32_t i = 0; i < showCount; i++) {
            printf("%u", trajectoryIds[i]);
            if (i < showCount - 1) printf(", ");
        }
        printf("]\n");
        if (header.NumTrajectoryIds > 10) {
            printf("  ... (%u more IDs)\n", header.NumTrajectoryIds - 10);
        }
    }

    // Check file size
    long currentPos = ftell(file);
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    
    long expectedSize = sizeof(SpecHeader) + 
                       header.NumEntries * sizeof(SpecEntry) + 
                       header.NumTrajectoryIds * sizeof(uint32_t);
    
    printf("\nFILE SIZE:\n");
    printf("  Actual: %ld bytes\n", fileSize);
    printf("  Expected: %ld bytes (64 + %u×16 + %u×4)\n", 
           expectedSize, header.NumEntries, header.NumTrajectoryIds);
    
    if (fileSize == expectedSize) {
        printf("✓ File size matches specification\n");
    } else {
        printf("❌ FAIL: File size mismatch!\n");
    }

    // Summary
    printf("\n=== VERIFICATION RESULT ===\n");
    if (header.Magic == 0x54534854 && 
        header.Version == 1 && 
        fileSize == expectedSize) {
        printf("✅ PASS: Binary format matches specification-spatial-hash-table.md\n");
    } else {
        printf("❌ FAIL: Binary format does NOT match specification\n");
    }

    fclose(file);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Usage: %s <timestep_file.bin>\n", argv[0]);
        printf("Example: %s /path/to/dataset/spatial_hashing/cellsize_10.000/timestep_00000.bin\n", argv[0]);
        return 1;
    }

    verifyBinaryFormat(argv[1]);
    return 0;
}
