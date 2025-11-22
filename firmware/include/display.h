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

// Set text at position (x, y) - characters are 4x5 pixels
void display_set_text(const char* text, int x, uint8_t y);

// Clear entire display
void display_clear(void);

// Update display (send framebuffer to displays)
void display_update(void);

// Display animation types
typedef enum {
    DISPLAY_ANIM_NONE = 0,
    DISPLAY_ANIM_BALL_SAVED,
    DISPLAY_ANIM_MULTIBALL,
    DISPLAY_ANIM_MAIN_MENU
} DisplayAnimation;

// Animation timing constants
#define BALL_SAVED_CYCLE_MS 333
#define MULTIBALL_SCROLL_START 32
#define MULTIBALL_SCROLL_DISTANCE 77

// Start a display animation
void display_start_animation(DisplayAnimation anim);

// Update display animations (call from main loop)
void display_update_animation(void);

// Debug test pattern (scroll "TEST", show digits, draw ball icons)
void display_test_pattern(void);

// Test pattern to draw an L in the bottom-right of each board
void display_draw_small_C_test(void);

#endif // DISPLAY_H
