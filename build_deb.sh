#!/bin/bash

set -e

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit 1
fi

echo "Starting package build process..."

# Install build dependencies
echo "Installing build dependencies..."
apt-get update
apt-get install -y devscripts debhelper dh-make build-essential

# Clean any previous build
echo "Cleaning previous build artifacts..."
rm -rf debian/genievm

# Check for VDDK installation
VDDK_PATH="/usr/local/vddk"
if [ ! -d "$VDDK_PATH" ]; then
    echo "Error: VDDK not found at $VDDK_PATH"
    echo "Please ensure VDDK is installed at $VDDK_PATH"
    echo "Expected structure:"
    echo "$VDDK_PATH/"
    echo "├── include/"
    echo "│   └── vixDiskLib.h"
    echo "└── lib64/"
    echo "    └── libvixDiskLib.so"
    exit 1
fi

# Verify VDDK files exist
if [ ! -f "$VDDK_PATH/include/vixDiskLib.h" ] || [ ! -f "$VDDK_PATH/lib64/libvixDiskLib.so" ]; then
    echo "Error: Required VDDK files not found in $VDDK_PATH"
    echo "Please ensure all VDDK files are properly installed"
    exit 1
fi

echo "VDDK files found at $VDDK_PATH, proceeding with build..."

# Show current directory
echo "Current directory: $(pwd)"
echo "Parent directory: $(dirname $(pwd))"

# Build the package
echo "Building package..."
debuild -us -uc

# List the created files
echo "Package built successfully!"
echo "Created files in parent directory:"
ls -lh ../genievm_*

echo "To install the package, run:"
echo "sudo dpkg -i ../genievm_1.0.0-1_amd64.deb" 