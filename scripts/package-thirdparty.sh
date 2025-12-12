#!/bin/bash
# Package built libraries into Unreal Engine ThirdParty layout

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

INSTALL_DIR="${PROJECT_ROOT}/install"
THIRDPARTY_DIR="${PROJECT_ROOT}/ThirdParty/trajectory_spatialhash"

if [ ! -d "${INSTALL_DIR}" ]; then
    echo "Error: Install directory not found: ${INSTALL_DIR}"
    echo "Please run build-release-libs.sh first"
    exit 1
fi

echo "Packaging libraries into ThirdParty layout..."
echo "  Source: ${INSTALL_DIR}"
echo "  Target: ${THIRDPARTY_DIR}"

# Create ThirdParty directory structure
mkdir -p "${THIRDPARTY_DIR}/include"
mkdir -p "${THIRDPARTY_DIR}/lib/win64"
mkdir -p "${THIRDPARTY_DIR}/lib/mac"
mkdir -p "${THIRDPARTY_DIR}/lib/linux"

# Copy headers
if [ -d "${INSTALL_DIR}/include/trajectory_spatialhash" ]; then
    echo "Copying headers..."
    cp -r "${INSTALL_DIR}/include/trajectory_spatialhash" "${THIRDPARTY_DIR}/include/"
fi

# Detect platform and copy appropriate libraries
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="mac"
    LIB_EXT="a"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
    PLATFORM="win64"
    LIB_EXT="lib"
else
    PLATFORM="linux"
    LIB_EXT="a"
fi

echo "Detected platform: ${PLATFORM}"
echo "Copying libraries..."

# Copy libraries for current platform
if [ -d "${INSTALL_DIR}/lib" ]; then
    find "${INSTALL_DIR}/lib" -name "*.${LIB_EXT}" -exec cp {} "${THIRDPARTY_DIR}/lib/${PLATFORM}/" \;
    find "${INSTALL_DIR}/lib" -name "*.a" -exec cp {} "${THIRDPARTY_DIR}/lib/${PLATFORM}/" \; 2>/dev/null || true
fi

echo ""
echo "Package complete!"
echo "ThirdParty layout created at: ${THIRDPARTY_DIR}"
echo ""
echo "Contents:"
find "${THIRDPARTY_DIR}" -type f

echo ""
echo "To use in Unreal Engine:"
echo "  1. Copy the ThirdParty folder to your plugin's directory"
echo "  2. Update the plugin's Build.cs to reference the libraries"
echo "  3. See examples/unreal_plugin/README.md for details"
