/**
 * hardware_config.h
 * 
 * GPIO pin definitions and hardware configuration for Pico pinball controller
 */

#ifndef HARDWARE_CONFIG_H
#define HARDWARE_CONFIG_H

// NeoPixel Configuration
#define NEOPIXEL_PIN 2
#define NEOPIXEL_COUNT 48
#define NEOPIXEL_BOARDS 6
#define LEDS_PER_BOARD 8

// I2C0 Bus Configuration (Shared: Arcade Seesaw Buttons + HT16K33 Matrix Displays)
#define I2C0_SDA_PIN 4
#define I2C0_SCL_PIN 5
#define I2C0_FREQ 100000

// I2C1 Bus Configuration (Haptics ONLY)
#define I2C1_SDA_PIN 6
#define I2C1_SCL_PIN 7
#define I2C1_FREQ 100000

// REMOVED: Software I2C Configuration for Matrix Displays
// Displays now share hardware I2C0 bus with Seesaw buttons
// GPIO8/9 are now available for other uses

// Misc GPIO
#define STATUS_LED_PIN 12
#define MODE_LED_PIN 13
#define BOOTSEL_TRIGGER_PIN 22

// Arcade Seesaw Button Board I2C Address (Adafruit 5296)
#define SEESAW_ADDR 0x3A

// HT16K33 Matrix Display I2C Addresses (Adafruit 1855 - 1.2" 8x8 with I2C Backpack)
// Connected on I2C0 bus (shared with Seesaw buttons)
#define MATRIX_ADDR_0 0x70  // OPEN
#define MATRIX_ADDR_1 0x71  // A0
#define MATRIX_ADDR_2 0x72  // A1
#define MATRIX_ADDR_3 0x73  // A1+A2

// DRV2605L Haptic I2C Addresses
#define HAPTIC_LEFT_ADDR 0x5A
#define HAPTIC_RIGHT_ADDR 0x5A

// Button Bit Masks (Seesaw)
#define BUTTON_LEFT_MASK (1 << 0)
#define BUTTON_CENTER_MASK (1 << 1)
#define BUTTON_RIGHT_MASK (1 << 2)

// Debug Mode
#define DEBUG_TIMEOUT_MS 30000  // 30 seconds

#endif // HARDWARE_CONFIG_H
