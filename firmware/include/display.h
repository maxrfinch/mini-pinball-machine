/**
 * display.h
 * 
 * HT16K33 8x8 matrix display driver
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

#endif // DISPLAY_H
