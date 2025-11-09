#!/bin/bash

set -e

echo "Installing C++ build dependencies..."
echo ""

if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Please install from https://brew.sh"
    exit 1
fi

echo "Homebrew found"
echo ""

echo "Installing CMake..."
brew install cmake || echo "CMake already installed or failed"

echo "Installing Boost..."
brew install boost || echo "Boost already installed or failed"

echo "Installing spdlog..."
brew install spdlog || echo "spdlog already installed or failed"

echo "Installing yaml-cpp..."
brew install yaml-cpp || echo "yaml-cpp already installed or failed"

echo "Installing nlohmann-json..."
brew install nlohmann-json || echo "nlohmann-json already installed or failed"

echo ""
echo "All dependencies installed."
echo ""
echo " Build: ./build.sh"
echo " Run: ./run.sh"
echo "  open http://localhost:9101/dual.html"
echo ""
