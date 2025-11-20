#!/bin/bash
# Build script for Pico firmware

# Check if PICO_SDK_PATH is set
if [ -z "$PICO_SDK_PATH" ]; then
    echo "Error: PICO_SDK_PATH environment variable is not set"
    echo "Please set it to the path of your Pico SDK installation"
    echo "Example: export PICO_SDK_PATH=/path/to/pico-sdk"
    exit 1
fi

# Create build directory if it doesn't exist
mkdir -p build

# Change to build directory
cd build

# Run CMake
echo "Running CMake..."
cmake ..

if [ $? -ne 0 ]; then
    echo "CMake configuration failed"
    exit 1
fi

# Build
echo "Building firmware..."
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo "Build failed"
    exit 1
fi

echo ""
echo "Build successful!"
echo "Firmware binary: build/pinball_firmware.uf2"
echo ""
echo "To flash:"
echo "1. Hold BOOTSEL button on Pico"
echo "2. Connect USB cable"
echo "3. Copy build/pinball_firmware.uf2 to the Pico drive"
echo "4. Pico will automatically reboot"
