/**
 * display.c
 * * HT16K33 8x8 matrix display driver for Adafruit 1.2" 8x8 LED Matrix with I2C Backpack
 * Uses hardware I2C0 bus (shared with Seesaw buttons)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
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

// Animation state
static DisplayAnimation current_animation = DISPLAY_ANIM_NONE;
static uint32_t animation_start_time = 0;
static uint32_t animation_frame = 0;

// Animation constants
#define TWO_PI 6.28318530718f

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

// 4x5 character font (A-Z, 0-9, space), 4 columns wide, 5 rows tall
// Each byte uses lowest 5 bits as vertical pixels (bit0 = top)
static const uint8_t char_font[37][4] = {
    {0b11111, 0b00101, 0b00101, 0b11111}, // A
    {0b11111, 0b10101, 0b10101, 0b01010}, // B
    {0b11111, 0b10001, 0b10001, 0b10001}, // C
    {0b11111, 0b10001, 0b10001, 0b01110}, // D
    {0b11111, 0b10101, 0b10101, 0b10001}, // E
    {0b11111, 0b00101, 0b00101, 0b00001}, // F
    {0b11111, 0b10001, 0b10101, 0b11101}, // G
    {0b11111, 0b00100, 0b00100, 0b11111}, // H
    {0b10001, 0b11111, 0b10001, 0b00000}, // I
    {0b11000, 0b10000, 0b10000, 0b11111}, // J
    {0b11111, 0b00100, 0b01010, 0b10001}, // K
    {0b11111, 0b10000, 0b10000, 0b10000}, // L
    {0b11111, 0b00010, 0b00010, 0b11111}, // M
    {0b11111, 0b00010, 0b00100, 0b11111}, // N
    {0b11111, 0b10001, 0b10001, 0b11111}, // O
    {0b11111, 0b00101, 0b00101, 0b00111}, // P
    {0b11111, 0b10001, 0b11001, 0b11111}, // Q
    {0b11111, 0b00101, 0b00101, 0b11010}, // R
    {0b10111, 0b10101, 0b10101, 0b11101}, // S
    {0b00001, 0b11111, 0b00001, 0b00000}, // T
    {0b11111, 0b10000, 0b10000, 0b11111}, // U
    {0b01111, 0b10000, 0b10000, 0b01111}, // V
    {0b11111, 0b10000, 0b10000, 0b11111}, // W
    {0b11011, 0b00100, 0b00100, 0b11011}, // X
    {0b00111, 0b11100, 0b11100, 0b00111}, // Y
    {0b11001, 0b10101, 0b10011, 0b10001}, // Z
    {0b11111, 0b10001, 0b10001, 0b11111}, // 0
    {0b00000, 0b11111, 0b00000, 0b00000}, // 1
    {0b11101, 0b10101, 0b10101, 0b10111}, // 2
    {0b10101, 0b10101, 0b10101, 0b11111}, // 3
    {0b00111, 0b00100, 0b00100, 0b11111}, // 4
    {0b10111, 0b10101, 0b10101, 0b11101}, // 5
    {0b11111, 0b10101, 0b10101, 0b11101}, // 6
    {0b00001, 0b00001, 0b00001, 0b11111}, // 7
    {0b11111, 0b10101, 0b10101, 0b11111}, // 8
    {0b10111, 0b10101, 0b10101, 0b11111}, // 9
    {0b00000, 0b00000, 0b00000, 0b00000}  // space
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
    cmd = HT16K33_BRIGHTNESS_CMD | 0x7;
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

// Draw a single character at position (x, y_phys) using 4x5 font
static int draw_char(uint8_t x, uint8_t y_phys, char c) {
    // Convert character to font index
    int index = -1;
    if (c >= 'A' && c <= 'Z') {
        index = c - 'A';
    } else if (c >= 'a' && c <= 'z') {
        index = c - 'a';  // Convert lowercase to uppercase
    } else if (c >= '0' && c <= '9') {
        index = 26 + (c - '0');
    } else if (c == ' ') {
        index = 36;
    }
    
    // Return 0 width if character not supported
    if (index < 0 || index >= 37) {
        return 0;
    }
    
    // Calculate character width (I is narrower)
    int char_width = (c == 'I' || c == 'i' || c == '1') ? 3 : 4;
    
    // Check bounds
    if (x + char_width > DISPLAY_WIDTH || y_phys + 5 > DISPLAY_HEIGHT) {
        return 0;
    }
    
    // Draw character
    for (int col = 0; col < char_width; col++) {
        uint8_t column_data = char_font[index][col];
        for (int row = 0; row < 5; row++) {
            if (column_data & (1 << row)) {
                uint8_t phys_row = y_phys + row;
                uint8_t fb_row = (phys_row + 7) % 8;
                
                if (x + col < DISPLAY_WIDTH) {
                    framebuffer[x + col][fb_row] = 1;
                }
            }
        }
    }
    
    return char_width;
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

void display_set_text(const char* text, uint8_t x, uint8_t y) {
    // Draw text starting at position (x, y)
    // y is the top physical row for the text (0-7)
    // Text will be drawn left-to-right with 1px spacing between characters
    
    if (!text || y + 5 > DISPLAY_HEIGHT) {
        return;
    }
    
    uint8_t cursor_x = x;
    for (int i = 0; text[i] != '\0' && cursor_x < DISPLAY_WIDTH; i++) {
        int char_width = draw_char(cursor_x, y, text[i]);
        cursor_x += char_width + 1;  // Add 1px spacing between characters
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

void display_start_animation(DisplayAnimation anim) {
    current_animation = anim;
    animation_start_time = to_ms_since_boot(get_absolute_time());
    animation_frame = 0;
    printf("[DISPLAY] Starting animation: %d\n", anim);
}

void display_update_animation(void) {
    if (current_animation == DISPLAY_ANIM_NONE) {
        return;
    }
    
    // Note on framebuffer row mapping:
    // Physical row N maps to framebuffer row (N+7)%8
    // Physical row 0 (top) = framebuffer[x][7]
    // Physical row 1 = framebuffer[x][0]
    // Physical row 2 = framebuffer[x][1]
    // ...
    // Physical row 7 (bottom) = framebuffer[x][6]
    
    uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - animation_start_time;
    animation_frame++;
    
    switch (current_animation) {
        case DISPLAY_ANIM_BALL_SAVED: {
            // Flash "BALL SAVED" text alternating with full brightness
            // 6 cycles over 2 seconds (333ms per cycle, 166ms on/off)
            uint32_t total_duration = 2000;
            if (elapsed_ms > total_duration) {
                current_animation = DISPLAY_ANIM_NONE;
                display_clear();
                break;
            }
            
            uint32_t cycle = elapsed_ms % BALL_SAVED_CYCLE_MS;
            bool show_text = (cycle < (BALL_SAVED_CYCLE_MS / 2));
            
            display_clear();
            if (show_text) {
                display_set_text("BALL", 0, 1);
                display_set_text("SAVED", 0, 6);
            } else {
                // Fill entire display
                for (int x = 0; x < DISPLAY_WIDTH; x++) {
                    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                        framebuffer[x][y] = 1;
                    }
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_MULTIBALL: {
            // Scrolling "MULTIBALL" text with trailing effect
            // Total duration: 4 seconds
            uint32_t total_duration = 4000;
            if (elapsed_ms > total_duration) {
                current_animation = DISPLAY_ANIM_NONE;
                display_clear();
                break;
            }
            
            display_clear();
            
            // Calculate scroll position (right to left)
            // Text is approximately 45 pixels wide (9 chars * 5 pixels each)
            // Start off-screen right (MULTIBALL_SCROLL_START), end off-screen left (x=-45)
            // Total travel: MULTIBALL_SCROLL_DISTANCE pixels over 4 seconds
            int scroll_offset = MULTIBALL_SCROLL_START - (int)((elapsed_ms * MULTIBALL_SCROLL_DISTANCE) / total_duration);
            
            display_set_text("MULTIBALL", scroll_offset, 2);
            
            // Add decorative dots on top and bottom rows
            for (int x = 0; x < DISPLAY_WIDTH; x += 4) {
                int offset_x = (x + (animation_frame / 2)) % DISPLAY_WIDTH;
                if (offset_x < DISPLAY_WIDTH) {
                    framebuffer[offset_x][7] = 1;  // Physical row 0 (top)
                    framebuffer[offset_x][6] = 1;  // Physical row 7 (bottom)
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_MAIN_MENU: {
            // Gentle pulsing border animation for main menu
            // Continuous until stopped
            
            display_clear();
            
            // Calculate pulse intensity (breathe over 2 second cycle)
            uint32_t cycle_ms = elapsed_ms % 2000;
            float pulse = (sinf((float)cycle_ms / 2000.0f * TWO_PI) + 1.0f) / 2.0f;  // 0.0 to 1.0
            
            // Draw border with gaps based on pulse
            // When pulse is high, show more border pixels
            int show_every = (pulse < 0.3f) ? 4 : (pulse < 0.6f) ? 3 : 2;
            
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                if (x % show_every == (animation_frame / 4) % show_every) {
                    framebuffer[x][7] = 1;  // Physical row 0 (top)
                    framebuffer[x][4] = 1;  // Physical row 5 (near bottom)
                }
            }
            
            // Vertical borders (just a few pixels)
            for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                if (y % show_every == (animation_frame / 4) % show_every) {
                    framebuffer[0][y] = 1;
                    framebuffer[DISPLAY_WIDTH - 1][y] = 1;
                }
            }
            
            // Draw "MENU" text in center
            display_set_text("MENU", 6, 1);
            break;
        }
        
        case DISPLAY_ANIM_NONE:
        default:
            break;
    }
}