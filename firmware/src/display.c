/**
 * display.c
 * * HT16K33 8x8 matrix display driver for Adafruit 1.2" 8x8 LED Matrix with I2C Backpack
 * Uses hardware I2C0 bus (shared with Seesaw buttons)
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

// 3x5 digit font (0–9), 3 columns wide, 5 rows tall
// Each byte uses lowest 5 bits as vertical pixels (bit0 = top)
static const uint8_t digit_font[10][3] = {
    {0b11111, 0b10001, 0b11111}, // 0
    {0b11111, 0b00000, 0b00000}, // 1
    {0b11101, 0b10101, 0b10111}, // 2
    {0b10101, 0b10101, 0b11111}, // 3
    {0b00111, 0b00100, 0b11111}, // 4
    {0b10111, 0b10101, 0b11101}, // 5
    {0b11111, 0b10101, 0b11101}, // 6
    {0b00001, 0b00001, 0b11111}, // 7
    {0b11111, 0b10101, 0b11111}, // 8
    {0b10111, 0b10101, 0b11111}  // 9
};

static bool ht16k33_write(uint8_t addr, const uint8_t* data, size_t len) {
    int ret = i2c_write_blocking(i2c0, addr, data, len, false);
    if (ret != len) {
        printf("HT16K33 0x%02X: I2C write failed (wrote %d/%zu bytes)\n", 
               addr, ret, len);
        return false;
    }
    return true;
}

static void ht16k33_init_display(uint8_t addr) {
    uint8_t cmd;
    bool success = true;
    
    printf("Initializing HT16K33 at 0x%02X...\n", addr);
    
    // Turn on oscillator
    cmd = HT16K33_SYSTEM_SETUP | HT16K33_OSCILLATOR_ON;
    if (!ht16k33_write(addr, &cmd, 1)) {
        printf("  Failed to turn on oscillator\n");
        success = false;
    }
    sleep_ms(1);
    
    // Set brightness to max
    cmd = HT16K33_BRIGHTNESS_CMD | 0x0;
    if (!ht16k33_write(addr, &cmd, 1)) {
        printf("  Failed to set brightness\n");
        success = false;
    }
    sleep_ms(1);
    
    // Turn on display, no blink
    cmd = HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON;
    if (!ht16k33_write(addr, &cmd, 1)) {
        printf("  Failed to turn on display\n");
        success = false;
    }
    sleep_ms(1);
    
    if (success) {
        printf("  HT16K33 0x%02X initialized successfully\n", addr);
    }
}

void display_init(void) {
    printf("\n=== Display Initialization (Shared I2C0 Hardware Bus) ===\n");
    printf("Matrix displays share I2C0 with Seesaw buttons\n");
    printf("I2C0 already initialized at %d Hz on GPIO%d (SDA) / GPIO%d (SCL)\n",
           I2C0_FREQ, I2C0_SDA_PIN, I2C0_SCL_PIN);
    
    sleep_ms(100);
    
    // Initialize framebuffer
    memset(framebuffer, 0, sizeof(framebuffer));
    
    // Initialize all displays
    printf("Initializing %d HT16K33 matrix displays on I2C0...\n", NUM_DISPLAYS);
    for (int i = 0; i < NUM_DISPLAYS; i++) {
        ht16k33_init_display(display_addrs[i]);
    }
    
    // Clear displays
    display_clear();
    display_update();
    
    printf("=== Display Initialization Complete ===\n\n");
}

void display_clear(void) {
    memset(framebuffer, 0, sizeof(framebuffer));
}

static void draw_digit(uint8_t x, uint8_t y_phys, uint8_t digit) {
    // y_phys is the top physical row for this 3x5 digit
    if (digit > 9 || x + 3 >= DISPLAY_WIDTH || y_phys + 5 > DISPLAY_HEIGHT) {
        return;
    }

    int digit_width = (digit == 1 ? 1 : 3);
    for (int col = 0; col < digit_width; col++) {
        uint8_t column_data = digit_font[digit][col];
        for (int row = 0; row < 5; row++) {
            if (column_data & (1 << row)) {
                uint8_t phys_row = y_phys + row;              // 0..7 physical
                uint8_t fb_row   = (phys_row + 7) % 8;        // map to framebuffer row

                if (x + col < DISPLAY_WIDTH) {
                    framebuffer[x + col][fb_row] = 1;
                }
            }
        }
    }
}

void display_set_score(uint32_t score) {
    // Clear score area: physical rows 0-4 → framebuffer rows 7,0,1,2,3
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        framebuffer[x][7] = 0; // physical row 0
        framebuffer[x][0] = 0; // physical row 1
        framebuffer[x][1] = 0; // physical row 2
        framebuffer[x][2] = 0; // physical row 3
        framebuffer[x][3] = 0; // physical row 4
    }

    // Convert score to string
    char score_str[12];
    snprintf(score_str, sizeof(score_str), "%lu", (unsigned long)score);
    
    // Draw digits left-aligned (increasing score grows to the right)
    int len = strlen(score_str);
    int x = 0;  // start at left edge

    for (int i = 0; i < len && x < DISPLAY_WIDTH; i++) {
        uint8_t digit = score_str[i] - '0';
        if (digit <= 9) {
            // Draw starting at physical top row 0
            draw_digit(x, 0, digit);
        }
        int w = (digit == 1 ? 1 : 3);
        x += w + 1; // digit width + 1 space
    }
}

void display_set_balls(uint8_t balls)
{
    // Layout:
    //   Score: physical rows 0–4
    //   Row 5: empty separator
    //   Balls: physical rows 6–7 (2×2 blocks, bottom‑right)

    // Clear physical rows 5, 6, 7
    for (int x = 0; x < DISPLAY_WIDTH; x++) {
        for (int phys = 5; phys <= 7; phys++) {
            uint8_t fb = (phys + 7) % 8;   // physical → framebuffer
            framebuffer[x][fb] = 0;
        }
    }

    // Draw balls from right to left, max 5
    for (int i = 0; i < balls && i < 5; i++) {
        int x_phys = DISPLAY_WIDTH - 2 - (i * 4);   // 2px ball + 2px gap

        for (int dx = 0; dx < 2; dx++) {
            int px = x_phys + dx;
            if (px < 0 || px >= DISPLAY_WIDTH) continue;

            // Ball occupies physical rows 6 and 7
            for (int dy = 0; dy < 2; dy++) {
                uint8_t phys = 6 + dy;            // 6,7
                uint8_t fb   = (phys + 7) % 8;
                framebuffer[px][fb] = 1;
            }
        }
    }
}

// ** TEST FUNCTION **
void display_draw_small_C_test(void) {
    display_clear();

    // Draw a small 3x3 'C' shape aligned to the top-left corner of each 8x8 board.
    for (int disp = 0; disp < NUM_DISPLAYS; disp++) {
        int x_offset = disp * 8; // Starting X coordinate for the current display (left edge)

        // NOTE: Due to the HT16K33 row mapping, physical top row corresponds to
        // framebuffer row 7, then rows wrap 0,1,... downward.

        // Vertical spine (left column of 3x3 block) on physical rows 0,1,2
        framebuffer[x_offset + 0][7] = 1; // physical row 0 (top)
        framebuffer[x_offset + 0][0] = 1; // physical row 1
        framebuffer[x_offset + 0][1] = 1; // physical row 2

        // Top bar (physical row 0, x = 1..2) -> framebuffer row 7
        framebuffer[x_offset + 1][7] = 1;
        framebuffer[x_offset + 2][7] = 1;

        // Bottom bar (physical row 2, x = 1..2) -> framebuffer row 1
        framebuffer[x_offset + 1][1] = 1;
        framebuffer[x_offset + 2][1] = 1;
    }
}

// ** FINAL MAPPING: Swap Axes + Register Flip + Bit Flip **
void display_update(void) {
    // Each display is 8x8, we have 4 displays side-by-side
    for (int disp = 0; disp < NUM_DISPLAYS; disp++) {
        uint8_t buffer[17];
        buffer[0] = 0x00; // Start at register 0
        
        // 1. OUTER LOOP: Iterate over Framebuffer Columns (X) 
        //    - This maps to the HT16K33 Register Address (Physical Row)
        for (int fb_col = 0; fb_col < 8; fb_col++) {
            uint8_t row_data = 0; 
            
            // *** FIX 1: Apply Horizontal Flip (to the Register Address) ***
            // Maps FB X=0 (left edge) to HT16K33 Register 7 (bottom physical row)
            // This handles the Horizontal orientation (X axis)
            int ht_reg_index = 7 - fb_col;
            
            // 2. INNER LOOP: Iterate over Framebuffer Rows (Y)
            //    - This maps to the HT16K33 Bit Position (Physical Column)
            for (int fb_row = 0; fb_row < 8; fb_row++) {
                int fb_x = disp * 8 + fb_col;

                if (framebuffer[fb_x][fb_row]) {
                    // Map framebuffer row directly to HT16K33 bit index
                    int ht_bit_index = fb_row;
                    row_data |= (1 << ht_bit_index);
                }
            }
            
            // Store the calculated byte using the flipped register index
            buffer[1 + ht_reg_index * 2] = row_data;
            buffer[2 + ht_reg_index * 2] = 0;
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
        int tx = 0;
        for (int i = 0; i < 10 && tx < DISPLAY_WIDTH; i++) {
            int w = (i == 1 ? 1 : 3);
            draw_digit(tx, 1, i);
            tx += w + 1;
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