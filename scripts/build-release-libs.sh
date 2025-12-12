#!/bin/bash
# Build release libraries for the current platform

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

BUILD_DIR="${PROJECT_ROOT}/build-release"
INSTALL_DIR="${PROJECT_ROOT}/install"

echo "Building trajectory_spatialhash release libraries..."
echo "  Build directory: ${BUILD_DIR}"
echo "  Install directory: ${INSTALL_DIR}"

# Clean previous build
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}"

cd "${BUILD_DIR}"

# Configure
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_EXAMPLES=OFF \
    -DCMAKE_INSTALL_PREFIX="${INSTALL_DIR}"

# Build
cmake --build . --config Release -j

# Install
cmake --install . --config Release

echo ""
echo "Build complete!"
echo "Libraries and headers installed to: ${INSTALL_DIR}"
echo ""
echo "Library files:"
find "${INSTALL_DIR}/lib" -type f 2>/dev/null || echo "  (none found)"
echo ""
echo "Header files:"
find "${INSTALL_DIR}/include" -type f 2>/dev/null || echo "  (none found)"
