#!/bin/bash

# Exit on error
set -e

# Verify VDDK paths
echo "Checking VDDK paths..."
if [ ! -d "/usr/local/vddk/include" ]; then
    echo "Error: VDDK include directory not found at /usr/local/vddk/include"
    exit 1
fi

if [ ! -f "/usr/local/vddk/include/vixDiskLib.h" ]; then
    echo "Error: vixDiskLib.h not found at /usr/local/vddk/include/vixDiskLib.h"
    exit 1
fi

# Check compiler version
echo "Checking compiler version..."
g++ --version

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Run CMake with the correct paths and verbose output
cmake -DVDDK_ROOT=/usr/local/vddk \
      -DVSPHERE_SDK_ROOT=/usr/local/vSphereSDK \
      -DCMAKE_VERBOSE_MAKEFILE=ON \
      -DCMAKE_CXX_COMPILER=g++ \
      -DCMAKE_CXX_FLAGS="-std=gnu++17" \
      ..

# Build the project with verbose output
make VERBOSE=1

echo "Build completed successfully!" 