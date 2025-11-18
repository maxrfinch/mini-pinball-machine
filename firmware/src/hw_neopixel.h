#ifndef HW_NEOPIXEL_H
#define HW_NEOPIXEL_H

#include <stdint.h>

// Phase 2 - NeoPixel control (65 LEDs: 64 game + 1 power)
// TODO: Implement in Phase 2

// Initialize NeoPixel hardware
void hw_neopixel_init(void);

// Set individual LED color
void hw_neopixel_set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b);

// Fill all LEDs with color
void hw_neopixel_fill(uint8_t r, uint8_t g, uint8_t b);

// Set animation mode
void hw_neopixel_mode(uint8_t mode_id, uint8_t *params, uint8_t param_count);

// Set power LED mode
void hw_neopixel_power_mode(uint8_t mode_id);

// Update NeoPixels (call from main loop)
void hw_neopixel_update(void);

#endif // HW_NEOPIXEL_H
