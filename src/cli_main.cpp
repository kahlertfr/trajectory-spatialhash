#include "trajectory_spatialhash/spatial_hash.h"
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

using namespace trajectory_spatialhash;

void PrintUsage(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " build <shard1.csv> [shard2.csv ...] -o <output.grid> [-c <cell_size>]\n";
    std::cout << "\nCommands:\n";
    std::cout << "  build    Build spatial hash grid from trajectory shard CSV files\n";
    std::cout << "\nOptions:\n";
    std::cout << "  -o <file>       Output grid file (required for build)\n";
    std::cout << "  -c <size>       Cell size for spatial hash (default: 10.0)\n";
    std::cout << "\nExample:\n";
    std::cout << "  " << prog_name << " build shard1.csv shard2.csv -o output.grid -c 5.0\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        PrintUsage(argv[0]);
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "build") {
        std::vector<const char*> shard_paths;
        const char* output_path = nullptr;
        double cell_size = 10.0;
        
        // Parse arguments
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0) {
                if (i + 1 < argc) {
                    output_path = argv[++i];
                } else {
                    std::cerr << "Error: -o requires an argument\n";
                    return 1;
                }
            } else if (std::strcmp(argv[i], "-c") == 0) {
                if (i + 1 < argc) {
                    try {
                        cell_size = std::stod(argv[++i]);
                        if (cell_size <= 0) {
                            std::cerr << "Error: cell size must be positive\n";
                            return 1;
                        }
                    } catch (...) {
                        std::cerr << "Error: invalid cell size\n";
                        return 1;
                    }
                } else {
                    std::cerr << "Error: -c requires an argument\n";
                    return 1;
                }
            } else if (argv[i][0] != '-') {
                shard_paths.push_back(argv[i]);
            } else {
                std::cerr << "Error: unknown option " << argv[i] << "\n";
                return 1;
            }
        }
        
        if (shard_paths.empty()) {
            std::cerr << "Error: no shard files specified\n";
            PrintUsage(argv[0]);
            return 1;
        }
        
        if (!output_path) {
            std::cerr << "Error: output file not specified (use -o)\n";
            PrintUsage(argv[0]);
            return 1;
        }
        
        std::cout << "Building spatial hash grid...\n";
        std::cout << "  Cell size: " << cell_size << "\n";
        std::cout << "  Shard files: " << shard_paths.size() << "\n";
        
        SpatialHashGrid grid(cell_size);
        
        if (!grid.BuildFromShards(shard_paths.data(), shard_paths.size())) {
            std::cerr << "Error: failed to build grid";
            if (const char* err = grid.GetLastError()) {
                std::cerr << ": " << err;
            }
            std::cerr << "\n";
            return 1;
        }
        
        std::cout << "  Points loaded: " << grid.GetPointCount() << "\n";
        
        std::cout << "Serializing to " << output_path << "...\n";
        
        if (!grid.Serialize(output_path)) {
            std::cerr << "Error: failed to serialize grid";
            if (const char* err = grid.GetLastError()) {
                std::cerr << ": " << err;
            }
            std::cerr << "\n";
            return 1;
        }
        
        std::cout << "Done!\n";
        return 0;
        
    } else {
        std::cerr << "Error: unknown command '" << command << "'\n";
        PrintUsage(argv[0]);
        return 1;
    }
}
