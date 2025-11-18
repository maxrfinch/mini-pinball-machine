#include "hw_display.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <string.h>
#include <stdio.h>

// HT16K33 I2C LED Driver addresses (for 4 matrices)
// Typical addresses: 0x70, 0x71, 0x72, 0x73
#define MATRIX_BASE_ADDR    0x70
#define MATRIX_COUNT        4

// HT16K33 registers
#define HT16K33_BLINK_CMD       0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BRIGHTNESS_CMD  0xE0
#define HT16K33_SYSTEM_SETUP    0x20
#define HT16K33_OSCILLATOR_ON   0x01

// I2C configuration (same as buttons)
#define I2C_PORT i2c0

// Framebuffer: 32 columns × 8 rows
static uint8_t framebuffer[DISPLAY_WIDTH];

// 5x5 digit font (for numbers 0-9)
// Each digit is 5 rows tall, 3-5 pixels wide
static const uint8_t digit_font[10][5] = {
    // 0
    {0b01110, 0b10001, 0b10001, 0b10001, 0b01110},
    // 1
    {0b00100, 0b01100, 0b00100, 0b00100, 0b01110},
    // 2
    {0b01110, 0b10001, 0b00110, 0b01000, 0b11111},
    // 3
    {0b01110, 0b10001, 0b00110, 0b10001, 0b01110},
    // 4
    {0b10001, 0b10001, 0b11111, 0b00001, 0b00001},
    // 5
    {0b11111, 0b10000, 0b11110, 0b00001, 0b11110},
    // 6
    {0b01110, 0b10000, 0b11110, 0b10001, 0b01110},
    // 7
    {0b11111, 0b00001, 0b00010, 0b00100, 0b01000},
    // 8
    {0b01110, 0b10001, 0b01110, 0b10001, 0b01110},
    // 9
    {0b01110, 0b10001, 0b01111, 0b00001, 0b01110}
};

// Digit widths (for proper spacing)
static const uint8_t digit_widths[10] = {5, 3, 5, 5, 5, 5, 5, 5, 5, 5};

void hw_display_init(void) {
    // I2C already initialized in hw_buttons_init()
    
    // Initialize all 4 matrix displays
    for (int i = 0; i < MATRIX_COUNT; i++) {
        uint8_t addr = MATRIX_BASE_ADDR + i;
        
        // Turn on oscillator
        uint8_t cmd = HT16K33_SYSTEM_SETUP | HT16K33_OSCILLATOR_ON;
        i2c_write_blocking(I2C_PORT, addr, &cmd, 1, false);
        
        // Set brightness to max (0-15)
        cmd = HT16K33_BRIGHTNESS_CMD | 0x0F;
        i2c_write_blocking(I2C_PORT, addr, &cmd, 1, false);
        
        // Turn on display, no blinking
        cmd = HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON;
        i2c_write_blocking(I2C_PORT, addr, &cmd, 1, false);
    }
    
    // Clear framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
}

void hw_display_set_pixel(uint8_t x, uint8_t y, bool on) {
    if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT) {
        return;
    }
    
    if (on) {
        framebuffer[x] |= (1 << y);
    } else {
        framebuffer[x] &= ~(1 << y);
    }
}

void hw_display_clear_rows(uint8_t start_row, uint8_t end_row) {
    if (start_row >= DISPLAY_HEIGHT || end_row >= DISPLAY_HEIGHT) {
        return;
    }
    
    uint8_t mask = 0xFF;
    // Create mask for rows to clear
    for (uint8_t row = start_row; row <= end_row; row++) {
        mask &= ~(1 << row);
    }
    
    // Apply mask to all columns
    for (uint8_t x = 0; x < DISPLAY_WIDTH; x++) {
        framebuffer[x] &= mask;
    }
}

void hw_display_clear(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

void hw_display_refresh(void) {
    // Update all 4 matrix displays
    // Each matrix is 8x8, displayed left to right
    for (int matrix = 0; matrix < MATRIX_COUNT; matrix++) {
        uint8_t addr = MATRIX_BASE_ADDR + matrix;
        uint8_t data[17]; // 1 byte address + 16 bytes data
        
        data[0] = 0x00; // Start at register 0
        
        // Each matrix shows 8 columns of the framebuffer
        int fb_start = matrix * 8;
        for (int col = 0; col < 8; col++) {
            int fb_col = fb_start + col;
            if (fb_col < DISPLAY_WIDTH) {
                // HT16K33 format: each register pair represents one column
                data[1 + col * 2] = framebuffer[fb_col];
                data[2 + col * 2] = 0x00;
            } else {
                data[1 + col * 2] = 0x00;
                data[2 + col * 2] = 0x00;
            }
        }
        
        i2c_write_blocking(I2C_PORT, addr, data, 17, false);
    }
}

void hw_display_render_score(uint32_t score) {
    // Clear score area (rows 0-4)
    hw_display_clear_rows(SCORE_ROW_START, SCORE_ROW_END);
    
    // Convert score to string
    char score_str[16];
    snprintf(score_str, sizeof(score_str), "%lu", (unsigned long)score);
    
    int len = strlen(score_str);
    if (len == 0) {
        hw_display_refresh();
        return;
    }
    
    // Calculate starting position (right-aligned)
    int x = DISPLAY_WIDTH - 1; // Start from rightmost column
    
    // Draw digits right to left
    for (int i = len - 1; i >= 0; i--) {
        char c = score_str[i];
        if (c >= '0' && c <= '9') {
            int digit = c - '0';
            int width = digit_widths[digit];
            
            // Draw the digit
            for (int col = width - 1; col >= 0; col--) {
                if (x >= 0 && x < DISPLAY_WIDTH) {
                    uint8_t col_data = digit_font[digit][col];
                    // Write to rows 0-4 (5 rows)
                    for (int row = 0; row < 5; row++) {
                        if (col_data & (1 << (4 - row))) {
                            hw_display_set_pixel(x, row, true);
                        }
                    }
                }
                x--;
            }
            
            // Add 1 pixel spacing between digits
            x--;
        }
    }
}

void hw_display_render_balls(uint8_t count) {
    // Clear ball area (rows 6-7)
    hw_display_clear_rows(BALL_ROW_START, BALL_ROW_END);
    
    if (count > 3) count = 3; // Max 3 balls displayed
    
    // Draw balls as 2x2 blocks, right-aligned
    // Ball spacing: 2x2 block + 2 columns gap
    int x = DISPLAY_WIDTH - 1; // Start from rightmost
    
    for (uint8_t i = 0; i < count; i++) {
        // Draw 2x2 block
        // Columns: x and x-1
        // Rows: 6 and 7
        if (x >= 1) {
            hw_display_set_pixel(x, 6, true);
            hw_display_set_pixel(x, 7, true);
            hw_display_set_pixel(x - 1, 6, true);
            hw_display_set_pixel(x - 1, 7, true);
            
            // Move left by 2 (block width) + 2 (gap)
            x -= 4;
        }
    }
}
