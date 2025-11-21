cmake_minimum_required(VERSION 3.13)

# Initialize the Pico SDK
include(pico_sdk_import.cmake)

project(pinball_firmware C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Add executable
add_executable(pinball_firmware
    src/main.c
    src/protocol.c
    src/neopixel.c
    src/onboard_neopixel.c
    src/buttons.c
    src/display.c
    src/debug_mode.c
)

# Generate PIO header for ws2812
pico_generate_pio_header(pinball_firmware ${CMAKE_CURRENT_LIST_DIR}/src/ws2812.pio)

# Add include directories
target_include_directories(pinball_firmware PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/include
)

# Link libraries
target_link_libraries(pinball_firmware
    pico_stdlib
    hardware_pio
    hardware_i2c
    hardware_gpio
    hardware_clocks
)

# Enable USB CDC for serial communication
pico_enable_stdio_usb(pinball_firmware 1)
pico_enable_stdio_uart(pinball_firmware 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(pinball_firmware)
