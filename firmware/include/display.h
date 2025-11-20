/**
 * display.h
 * 
 * HT16K33 8x8 matrix display driver for Adafruit 1.2" 8x8 LED Matrix with I2C Backpack
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

// Initialize display subsystem
void display_init(void);

// Set score value
void display_set_score(uint32_t score);

// Set ball count
void display_set_balls(uint8_t balls);

// Clear entire display
void display_clear(void);

// Update display (send framebuffer to displays)
void display_update(void);

// Debug test pattern (scroll "TEST", show digits, draw ball icons)
void display_test_pattern(void);

// Test pattern to draw an L in the bottom-right of each board
void display_draw_small_C_test(void);

#endif // DISPLAY_H
