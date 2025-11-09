#!/bin/bash

set -e

echo "Building ETH-sim C++..."
echo ""

if ! command -v cmake &> /dev/null; then
    echo "CMake not found. Please run: ./INSTALL_DEPS.sh"
    exit 1
fi

if [ -d "build" ]; then
    echo "Cleaning previous build..."
    rm -rf build
fi

echo "Configuring CMake..."
cmake -B build -DCMAKE_BUILD_TYPE=Release

echo "Building..."
cmake --build build --parallel

echo ""
echo "Build complete"
echo ""
echo "  Terminal 1: ./build/src/dex_sim/dex-sim"
echo "  Terminal 2: ./build/src/oracle_sim/oracle-sim"
echo "  Browser:    open http://localhost:9101/dual.html"
echo ""
