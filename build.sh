#!/bin/bash

# Exit on error
set -e

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    echo "Please do not run this script as root"
    exit 1
fi

# Check for required packages
echo "Checking required packages..."
if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Please install required packages first:"
    echo "sudo dnf groupinstall 'Development Tools'"
    echo "sudo dnf install cmake openssl-devel gcc-c++ make"
    exit 1
fi

# Check for VDDK
if [ -z "$VDDK_HOME" ]; then
    echo "VDDK_HOME environment variable not set"
    echo "Please set VDDK_HOME to your VDDK installation directory"
    echo "Example: export VDDK_HOME=/usr/local/vddk"
    exit 1
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DVDDK_ROOT=$VDDK_HOME

# Build the project
echo "Building the project..."
make -j$(nproc)

echo "Build completed successfully!"
echo "You can find the executables in the build directory" 