/**
 * display.c
 * 
 * HT16K33 8x8 matrix display driver for Adafruit 1.2" 8x8 LED Matrix with I2C Backpack
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "display.h"
#include "hardware_config.h"

// HT16K33 commands
#define HT16K33_BLINK_CMD 0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BRIGHTNESS_CMD 0xE0
#define HT16K33_SYSTEM_SETUP 0x20
#define HT16K33_OSCILLATOR_ON 0x01

// Display configuration
#define DISPLAY_WIDTH 32
#define DISPLAY_HEIGHT 8
#define NUM_DISPLAYS 4

static const uint8_t display_addrs[NUM_DISPLAYS] = {
    MATRIX_ADDR_0,
    MATRIX_ADDR_1,
    MATRIX_ADDR_2,
    MATRIX_ADDR_3
};

// Framebuffer (32x8 = 256 pixels)
static uint8_t framebuffer[DISPLAY_WIDTH][DISPLAY_HEIGHT];

// 5x5 digit font (0-9)
static const uint8_t digit_font[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
};

static bool ht16k33_write(uint8_t addr, const uint8_t* data, size_t len) {
    int ret = i2c_write_blocking(i2c0, addr, data, len, false);
    return ret == len;
}

static void ht16k33_init_display(uint8_t addr) {
    uint8_t cmd;
    
    // Turn on oscillator
    cmd = HT16K33_SYSTEM_SETUP | HT16K33_OSCILLATOR_ON;
    ht16k33_write(addr, &cmd, 1);
    sleep_ms(1);
    
    // Set brightness to max
    cmd = HT16K33_BRIGHTNESS_CMD | 0x0F;
    ht16k33_write(addr, &cmd, 1);
    sleep_ms(1);
    
    // Turn on display, no blink
    cmd = HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON;
    ht16k33_write(addr, &cmd, 1);
    sleep_ms(1);
}

void display_init(void) {
    // I2C0 already initialized by buttons
    
    // Initialize framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
    
    // Initialize all displays
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        ht16k33_init_display(display_addrs[i]);
    }
    
    // Clear displays
    display_clear();
    display_update();
}

void display_clear(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

static void draw_digit(uint8_t x, uint8_t y, uint8_t digit) {
    if (digit > 9 || x + 3 >= DISPLAY_WIDTH || y + 5 > DISPLAY_HEIGHT) {
        return;
    }
    
    for (int col = 0; col < 5; col++) {
        uint8_t column_data = digit_font[digit][col];
        for (int row = 0; row < 5; row++) {
            if (column_data & (1 << row)) {
                if (x + col < DISPLAY_WIDTH && y + row < DISPLAY_HEIGHT) {
                    framebuffer[x + col][y + row] = 1;
                }
            }
        }
    }
}

void display_set_score(uint32_t score) {
    // Clear score area (rows 0-4)
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        for (int y = 0; y < 5; y++) {
            framebuffer[x][y] = 0;
        }
    }
    
    // Convert score to string
    char score_str[12];
    snprintf(score_str, sizeof(score_str), "%lu", (unsigned long)score);
    
    // Draw digits right-aligned
    int len = strlen(score_str);
    int x = DISPLAY_WIDTH - (len * 4);  // 3 pixels per digit + 1 space
    if (x < 0) x = 0;
    
    for (int i = 0; i < len && x < DISPLAY_WIDTH; i++) {
        uint8_t digit = score_str[i] - '0';
        if (digit <= 9) {
            draw_digit(x, 0, digit);
        }
        x += 4; // 3 pixels + 1 space
    }
}

void display_set_balls(uint8_t balls) {
    // Clear ball area (rows 6-7)
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        for (int y = 6; y < DISPLAY_HEIGHT; y++) {
            framebuffer[x][y] = 0;
        }
    }
    
    // Draw 2x2 blocks from right to left
    for (int i = 0; i < balls && i < 5; i++) {
        int x = DISPLAY_WIDTH - 2 - (i * 4); // 2 pixels + 2 space per ball
        
        // Draw 2x2 block
        for (int dx = 0; dx < 2; dx++) {
            for (int dy = 0; dy < 2; dy++) {
                if (x + dx < DISPLAY_WIDTH) {
                    framebuffer[x + dx][6 + dy] = 1;
                }
            }
        }
    }
}

void display_update(void) {
    // Each display is 8x8, we have 4 displays side-by-side
    for (int disp = 0; disp < NUM_DISPLAYS; disp++) {
        uint8_t buffer[17]; // 0x00 register address + 16 bytes data
        buffer[0] = 0x00;   // Start at register 0
        
        // Convert framebuffer to display format
        for (int row = 0; row < 8; row++) {
            uint8_t row_data = 0;
            for (int col = 0; col < 8; col++) {
                int fb_x = disp * 8 + col;
                if (framebuffer[fb_x][row]) {
                    row_data |= (1 << col);
                }
            }
            buffer[1 + row * 2] = row_data;
            buffer[2 + row * 2] = 0;
        }
        
        ht16k33_write(display_addrs[disp], buffer, 17);
    }
}

void display_test_pattern(void) {
    static uint32_t test_frame = 0;
    test_frame++;
    
    display_clear();
    
    // Cycle through different test patterns
    uint8_t pattern = (test_frame / 50) % 3;
    
    if (pattern == 0) {
        // Show all digits
        for (int i = 0; i < 10 && i * 4 < DISPLAY_WIDTH; i++) {
            draw_digit(i * 4, 1, i);
        }
    } else if (pattern == 1) {
        // Draw ball icons
        display_set_balls(3);
    } else {
        // Scrolling test
        uint8_t scroll_pos = (test_frame / 10) % DISPLAY_WIDTH;
        if (scroll_pos < DISPLAY_WIDTH) {
            for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                framebuffer[scroll_pos][y] = 1;
            }
        }
    }
}
