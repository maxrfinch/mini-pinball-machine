#ifndef HW_DISPLAY_H
#define HW_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>

// Display dimensions
#define DISPLAY_WIDTH   32
#define DISPLAY_HEIGHT  8

// Row ranges
#define SCORE_ROW_START 0
#define SCORE_ROW_END   4
#define BALL_ROW_START  6
#define BALL_ROW_END    7

// Initialize matrix display hardware
void hw_display_init(void);

// Update the physical displays with current framebuffer
void hw_display_refresh(void);

// Set a pixel in the framebuffer
void hw_display_set_pixel(uint8_t x, uint8_t y, bool on);

// Clear a row range
void hw_display_clear_rows(uint8_t start_row, uint8_t end_row);

// Clear entire display
void hw_display_clear(void);

// Render score (right-aligned in rows 0-4)
void hw_display_render_score(uint32_t score);

// Render ball count (2x2 blocks in rows 6-7, right-aligned)
void hw_display_render_balls(uint8_t count);

#endif // HW_DISPLAY_H
