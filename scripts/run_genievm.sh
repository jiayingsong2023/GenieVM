#!/bin/bash

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

# Check if the executable exists
if [ ! -f "$BUILD_DIR/genievm" ]; then
    echo "Error: genievm executable not found in $BUILD_DIR"
    echo "Please run ./build.sh first"
    exit 1
fi

# Set library paths
export LD_LIBRARY_PATH="$BUILD_DIR:/usr/lib/x86_64-linux-gnu:$LD_LIBRARY_PATH"
export LD_PRELOAD="/usr/lib/x86_64-linux-gnu/libstdc++.so.6"

# Run the executable
"$BUILD_DIR/genievm" "$@" 