#!/bin/bash

# Build script for libxpdk

set -e

# Configuration
BUILD_TYPE=${BUILD_TYPE:-Release}
BUILD_DIR=${BUILD_DIR:-build}
INSTALL_PREFIX=${INSTALL_PREFIX:-/usr/local}

echo "Building libxpdk..."
echo "Build type: $BUILD_TYPE"
echo "Build directory: $BUILD_DIR"
echo "Install prefix: $INSTALL_PREFIX"

# Create build directory
mkdir -p $BUILD_DIR
cd $BUILD_DIR

# Configure with CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX=$INSTALL_PREFIX \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_TESTS=ON

# Build
make -j$(nproc)

echo "Build completed successfully!"
echo ""
echo "To install, run: make install"
echo "To run tests, run: make test"
echo ""
echo "Example binaries are in: $BUILD_DIR/examples/"
echo "Test binaries are in: $BUILD_DIR/tests/"
