#!/bin/bash

# Store the root directory path
ROOT_DIR=$(pwd)

# Create build directory if it doesn't exist
mkdir -p build

# Navigate to build directory
cd build

# Run cmake (if needed) and make
# cmake ..
make

# Run the executable
./bin/main

# Return to root directory
cd "$ROOT_DIR"