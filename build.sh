#!/bin/bash

# Exit on error
set -e

# Save original LD_LIBRARY_PATH
ORIGINAL_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
ORIGINAL_LD_PRELOAD=$LD_PRELOAD

# Clear VDDK-related environment variables for build
unset LD_LIBRARY_PATH
unset LD_PRELOAD

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
    echo "Checking VDDK paths..."
    if [ ! -d "/usr/local/vddk" ]; then
        echo "Error: VDDK not found at /usr/local/vddk"
        exit 1
    fi

    echo "Checking compiler version..."
    g++ --version

    # Create build directory if it doesn't exist
    mkdir -p build
    cd build

    # Run CMake with system libraries
    cmake ..

    # Build the project
    make -j$(nproc)

    # Restore original environment variables
    export LD_LIBRARY_PATH=$ORIGINAL_LD_LIBRARY_PATH
    export LD_PRELOAD=$ORIGINAL_LD_PRELOAD

    # Make the run script executable
    chmod +x ../scripts/run_genievm.sh

    echo "Build completed. Use ./scripts/run_genievm.sh to run the program."
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