/**
 * neopixel.h
 * 
 * NeoPixel LED control driver
 */

#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include "types.h"

// Initialize NeoPixel subsystem
void neopixel_init(void);

// Set brightness (0-255)
void neopixel_set_brightness(uint8_t brightness);

// Set a single LED color
void neopixel_set_led(uint8_t index, Color color);

// Set all LEDs to same color
void neopixel_fill(Color color);

// Clear all LEDs
void neopixel_clear(void);

// Update LED display (send data to strip)
void neopixel_show(void);

// Start an effect pattern
void neopixel_start_effect(LedEffect effect);

// Update effect animation (call periodically)
void neopixel_update_effect(void);

// Get board configuration
const LedBoard* neopixel_get_board(uint8_t board_id);

#endif // NEOPIXEL_H
