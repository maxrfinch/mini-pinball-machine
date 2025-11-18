#!/bin/bash
# Build script for Pico firmware

# Check if PICO_SDK_PATH is set
if [ -z "$PICO_SDK_PATH" ]; then
    echo "Error: PICO_SDK_PATH environment variable is not set"
    echo "Please set it to the path of your Pico SDK installation"
    echo "Example: export PICO_SDK_PATH=/path/to/pico-sdk"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
echo "Configuring build..."
cmake ..

# Build
echo "Building firmware..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo ""
    echo "Build successful!"
    echo "Flash pinball_firmware.uf2 to your Pico by holding BOOTSEL while connecting USB"
    echo "The .uf2 file is located at: build/pinball_firmware.uf2"
else
    echo ""
    echo "Build failed!"
    exit 1
fi
