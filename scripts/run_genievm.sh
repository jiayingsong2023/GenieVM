#!/bin/bash

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Set VDDK paths
export VDDK_ROOT="/usr/local/vddk"
export VDDK_LIB_DIR="$VDDK_ROOT/lib64"

# Set library paths
export LD_LIBRARY_PATH="$PROJECT_ROOT/build:$VDDK_LIB_DIR:$LD_LIBRARY_PATH"

# Set environment variables for VDDK
export LD_PRELOAD="$VDDK_LIB_DIR/libstdc++.so.6"

# Run genievm with all arguments passed to this script
"$PROJECT_ROOT/build/genievm" "$@" 