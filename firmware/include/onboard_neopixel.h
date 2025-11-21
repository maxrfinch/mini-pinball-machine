/**
 * onboard_neopixel.h
 * 
 * KB2040 onboard NeoPixel control (GPIO 17)
 * Single RGB LED with cycling animations
 */

#ifndef ONBOARD_NEOPIXEL_H
#define ONBOARD_NEOPIXEL_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// Initialize onboard NeoPixel
void onboard_neopixel_init(void);

// Update animation (call periodically in main loop)
void onboard_neopixel_update(void);

// Set to debug mode (steady orange using HSL)
void onboard_neopixel_set_debug_mode(bool enabled);

// Set brightness (0-255)
void onboard_neopixel_set_brightness(uint8_t brightness);

#endif // ONBOARD_NEOPIXEL_H
