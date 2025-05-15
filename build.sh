#!/bin/bash

# Exit on error
set -e

# Function to clean build directory
clean() {
    echo "Cleaning build directory..."
    if [ -d "build" ]; then
        rm -rf build/*
        echo "Build directory cleaned."
    else
        echo "Build directory does not exist."
    fi
}

# Function to build the project
build() {
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
}

# Function to rebuild (clean + build)
rebuild() {
    clean
    build
}

# Main script logic
case "$1" in
    "clean")
        clean
        ;;
    "rebuild")
        rebuild
        ;;
    "build"|"")
        build
        ;;
    *)
        echo "Usage: $0 {build|clean|rebuild}"
        echo "  build   - Build the project (default)"
        echo "  clean   - Clean the build directory"
        echo "  rebuild - Clean and rebuild the project"
        exit 1
        ;;
esac 