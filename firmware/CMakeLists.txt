cmake_minimum_required(VERSION 3.13)

# Include Pico SDK (assumes PICO_SDK_PATH is set)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_pinball_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(pinball_firmware
    src/main.c
    src/hw_serial.c
    src/hw_buttons.c
    src/hw_display.c
    src/protocol.c
)

# Pull in common dependencies
target_link_libraries(pinball_firmware
    pico_stdlib
    hardware_i2c
)

# Enable USB output, disable UART output
pico_enable_stdio_usb(pinball_firmware 1)
pico_enable_stdio_uart(pinball_firmware 0)

# Create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(pinball_firmware)

# Add compile options
target_compile_options(pinball_firmware PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)
