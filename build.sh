#!/bin/bash

# Exit on error
set -e

# Save original LD_LIBRARY_PATH
ORIGINAL_LD_LIBRARY_PATH=$LD_LIBRARY_PATH
ORIGINAL_LD_PRELOAD=$LD_PRELOAD

# Set to Debug Mode
export CMAKE_BUILD_TYPE=Debug
export CMAKE_CXX_FLAGS="-g -O0 -Wall -Wextra -DDEBUG" 

# Set VDDK library path
export LD_LIBRARY_PATH="/usr/local/vddk/lib64:$LD_LIBRARY_PATH"

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

    # Check if debug build is requested
    if [ "$DEBUG" = "1" ]; then
        echo "Building in debug mode..."
        cmake -DCMAKE_BUILD_TYPE=Debug \
              -DCMAKE_CXX_FLAGS_DEBUG="-g -O0 -Wall -Wextra -DDEBUG" \
              ..
    else
        # Run CMake with system libraries
        cmake ..
    fi

    # Build the project
    make -j$(nproc)

    # Restore original environment variables
    export LD_LIBRARY_PATH=$ORIGINAL_LD_LIBRARY_PATH
    export LD_PRELOAD=$ORIGINAL_LD_PRELOAD

    # Make the run script executable
    chmod +x ../scripts/run_genievm.sh

    if [ "$DEBUG" = "1" ]; then
        echo "Debug build completed. Use ./scripts/run_genievm.sh to run the program."
        echo "For debugging, run: gdb ./build/genievm"
    else
        echo "Build completed. Use ./scripts/run_genievm.sh to run the program."
    fi
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
    "debug")
        export DEBUG=1
        rebuild
        ;;
    "build"|"")
        build
        ;;
    *)
        echo "Usage: $0 {build|clean|rebuild|debug}"
        echo "  build   - Build the project (default)"
        echo "  clean   - Clean the build directory"
        echo "  rebuild - Clean and rebuild the project"
        echo "  debug   - Clean and rebuild with debug symbols"
        exit 1
        ;;
esac 
