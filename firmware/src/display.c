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
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TWO_PI (2.0f * M_PI)

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
static int draw_char(int x, uint8_t y_phys, char c) {
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
    
    // Check vertical bounds (horizontal is clipped per pixel)
    if (y_phys + 5 > DISPLAY_HEIGHT) {
        return 0;
    }

    // Draw character with per-pixel horizontal clipping
    for (int col = 0; col < char_width; col++) {
        uint8_t column_data = char_font[index][col];
        for (int row = 0; row < 5; row++) {
            if (column_data & (1 << row)) {
                uint8_t phys_row = y_phys + row;
                uint8_t fb_row   = (phys_row + 7) % 8;

                int px = x + col;
                if (px >= 0 && px < DISPLAY_WIDTH) {
                    framebuffer[px][fb_row] = 1;
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

void display_set_text(const char* text, int x, uint8_t y) {
    // Draw text starting at position (x, y)
    // y is the top physical row for the text (0-7)
    // Text will be drawn left-to-right with 1px spacing between characters
    
    if (!text || y + 5 > DISPLAY_HEIGHT) {
        return;
    }

    int cursor_x = x;
    for (int i = 0; text[i] != '\0'; i++) {
        // If we've scrolled completely off the right, stop
        if (cursor_x >= DISPLAY_WIDTH) {
            break;
        }

        int char_width = draw_char(cursor_x, y, text[i]);
        cursor_x += char_width + 1;  // 1px spacing between characters
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
            const char *text = "MULTIBALL";

            uint32_t total_duration = 4000;
            if (elapsed_ms > total_duration) {
                current_animation = DISPLAY_ANIM_NONE;
                display_clear();
                break;
            }

            display_clear();

            // These must match how display_set_text() lays characters out
            const int CHAR_WIDTH   = 4;  // columns in font
            const int CHAR_SPACING = 1;  // space between characters

            int len = (int)strlen(text);
            int text_width = len * (CHAR_WIDTH + CHAR_SPACING) - CHAR_SPACING;

            int scroll_start = DISPLAY_WIDTH;            // just off right edge
            int scroll_end   = -text_width;             // just off left edge
            int scroll_range = scroll_start - scroll_end; // positive distance

            int scroll_offset = scroll_start -
                (int)((scroll_range * (int32_t)elapsed_ms) / (int32_t)total_duration);

            display_set_text(text, scroll_offset, 2);

            // Decorative dots
            for (int x = 0; x < DISPLAY_WIDTH; x += 4) {
                int offset_x = (x + (animation_frame / 2)) % DISPLAY_WIDTH;
                framebuffer[offset_x][7] = 1;  // top
                framebuffer[offset_x][6] = 1;  // bottom
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
        
        case DISPLAY_ANIM_ICED_UP: {
            // Timed animation (~6 seconds)
            // A small 3x4 character sprite centered, shivers left/right
            // Snow falls from above
            uint32_t total_duration = 6000;
            if (elapsed_ms > total_duration) {
                current_animation = DISPLAY_ANIM_NONE;
                display_clear();
                break;
            }
            
            display_clear();
            
            // Character position (centered on 32x8 display)
            int char_base_x = 14;  // Center x for 3-wide character
            int char_y = 2;         // Starting y position (physical rows)
            
            // Shiver offset (alternates -1, 0, +1)
            int shiver_offset = 0;
            if ((animation_frame / 8) % 3 == 0) {
                shiver_offset = -1;
            } else if ((animation_frame / 8) % 3 == 1) {
                shiver_offset = 1;
            }
            
            int char_x = char_base_x + shiver_offset;
            
            // Character body frame (alternates between two frames)
            bool arms_out = (animation_frame / 12) % 2 == 0;
            
            // Draw character (3x4 sprite)
            // Simple stick figure
            // Frame 1 (arms in):  Frame 2 (arms out):
            //   O                    \O/
            //   |                     |
            //   |                     |
            
            // Head (top row)
            int head_y = (char_y + 7) % 8;
            framebuffer[char_x + 1][head_y] = 1;
            
            // Body (middle two rows)
            int body_y1 = (char_y + 1 + 7) % 8;
            int body_y2 = (char_y + 2 + 7) % 8;
            framebuffer[char_x + 1][body_y1] = 1;
            framebuffer[char_x + 1][body_y2] = 1;
            
            // Arms (if arms_out)
            if (arms_out) {
                framebuffer[char_x][body_y1] = 1;
                framebuffer[char_x + 2][body_y1] = 1;
            }
            
            // Snowfall density: ramps up, stays steady, tapers off
            float density;
            if (elapsed_ms < 1000) {
                density = (float)elapsed_ms / 1000.0f;
            } else if (elapsed_ms < 5000) {
                density = 1.0f;
            } else {
                density = (6000.0f - (float)elapsed_ms) / 1000.0f;
            }
            
            // Spawn snow pixels above and around character
            int num_snow = (int)(density * 8.0f);
            for (int i = 0; i < num_snow; i++) {
                // Random column around character area
                int snow_x = char_base_x - 3 + ((animation_frame * 7 + i * 13) % 9);
                // Snow drifts down based on frame
                int snow_fall = (animation_frame + i * 3) % 8;
                int snow_y_phys = snow_fall;
                int snow_y_fb = (snow_y_phys + 7) % 8;
                
                if (snow_x >= 0 && snow_x < DISPLAY_WIDTH && snow_y_phys < char_y) {
                    framebuffer[snow_x][snow_y_fb] = 1;
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_MULTIBALL_DAZZLE: {
            // Continuous overlay (does not clear score)
            // Border chase + interior sparkles
            
            // Border chase - clockwise around 32x8 matrix
            int border_len = (DISPLAY_WIDTH * 2) + (DISPLAY_HEIGHT * 2) - 4;
            int chase_pos = animation_frame % border_len;
            
            // Calculate position along border
            int bx, by_fb;
            if (chase_pos < DISPLAY_WIDTH) {
                // Top edge (left to right)
                bx = chase_pos;
                by_fb = 7;  // Physical row 0
            } else if (chase_pos < DISPLAY_WIDTH + DISPLAY_HEIGHT - 1) {
                // Right edge (top to bottom)
                bx = DISPLAY_WIDTH - 1;
                int phys_y = chase_pos - DISPLAY_WIDTH + 1;
                by_fb = (phys_y + 7) % 8;
            } else if (chase_pos < DISPLAY_WIDTH * 2 + DISPLAY_HEIGHT - 1) {
                // Bottom edge (right to left)
                bx = DISPLAY_WIDTH - 1 - (chase_pos - DISPLAY_WIDTH - DISPLAY_HEIGHT + 1);
                by_fb = 6;  // Physical row 7
            } else {
                // Left edge (bottom to top)
                int phys_y = 7 - (chase_pos - DISPLAY_WIDTH * 2 - DISPLAY_HEIGHT + 1);
                bx = 0;
                by_fb = (phys_y + 7) % 8;
            }
            
            framebuffer[bx][by_fb] = 1;
            
            // Interior sparkles (3-4 random positions per frame)
            // Avoid score area (top 5 rows)
            for (int i = 0; i < 4; i++) {
                int sx = ((animation_frame * 17 + i * 23) % 28) + 2;  // Interior columns
                int sy_phys = 5 + ((animation_frame * 11 + i * 19) % 3);  // Rows 5-7
                int sy_fb = (sy_phys + 7) % 8;
                
                if (sx >= 1 && sx < DISPLAY_WIDTH - 1) {
                    framebuffer[sx][sy_fb] = 1;
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_CENTER_WATERFALL: {
            // Continuous waterfall effect in center columns (13-18)
            // Alternates even/odd pixels for shimmering
            
            bool even_frame = (animation_frame % 2) == 0;
            
            for (int x = 13; x < 19 && x < DISPLAY_WIDTH; x++) {
                for (int phys_y = 0; phys_y < 8; phys_y++) {
                    // Alternate even/odd based on frame and position
                    bool show_pixel = even_frame ? ((phys_y % 2) == 0) : ((phys_y % 2) == 1);
                    
                    if (show_pixel) {
                        int fb_y = (phys_y + 7) % 8;
                        framebuffer[x][fb_y] = 1;
                    }
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_WATER_RIPPLE: {
            // Companion to CENTER_WATERFALL
            // Topmost row of water region with shifting checkerboard
            
            // Water surface is typically at bottom rows (6-7)
            int water_surface_phys = 6;
            int fb_y = (water_surface_phys + 7) % 8;
            
            bool phase = (animation_frame / 2) % 2 == 0;
            
            for (int x = 0; x < DISPLAY_WIDTH; x++) {
                bool show = phase ? ((x % 2) == 0) : ((x % 2) == 1);
                if (show) {
                    framebuffer[x][fb_y] = 1;
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_GAME_OVER_CURTAIN: {
            // One-shot animation (~1-2 seconds)
            // Two curtains close from edges to center
            
            uint32_t total_duration = 1500;
            uint32_t hold_duration = 200;
            
            if (elapsed_ms > total_duration + hold_duration) {
                current_animation = DISPLAY_ANIM_NONE;
                display_clear();
                break;
            }
            
            display_clear();
            
            if (elapsed_ms <= total_duration) {
                // Closing phase
                int columns_filled = (int)((elapsed_ms * DISPLAY_WIDTH / 2) / total_duration);
                
                // Fill from left
                for (int x = 0; x < columns_filled && x < DISPLAY_WIDTH / 2; x++) {
                    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                        framebuffer[x][y] = 1;
                    }
                }
                
                // Fill from right
                for (int x = DISPLAY_WIDTH - 1; x > DISPLAY_WIDTH - 1 - columns_filled && x >= DISPLAY_WIDTH / 2; x--) {
                    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                        framebuffer[x][y] = 1;
                    }
                }
            } else {
                // Hold phase - full screen lit
                for (int x = 0; x < DISPLAY_WIDTH; x++) {
                    for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                        framebuffer[x][y] = 1;
                    }
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_HIGH_SCORE: {
            // Repeating celebratory animation
            // Score digits pop one at a time with strobing
            
            // Use a 4-second loop
            uint32_t loop_duration = 4000;
            uint32_t loop_time = elapsed_ms % loop_duration;
            
            // Placeholder: simulate score "12345"
            const char* score_text = "12345";
            int num_digits = 5;
            
            // Each digit gets 500ms to appear and strobe
            int digit_time = 500;
            int current_digit = loop_time / digit_time;
            
            display_clear();
            
            if (current_digit < num_digits) {
                // Pop digits sequentially with strobe
                for (int i = 0; i <= current_digit && i < num_digits; i++) {
                    int digit = score_text[i] - '0';
                    int x = i * 4;
                    
                    // Strobe the current digit being added
                    if (i == current_digit) {
                        bool show = (animation_frame % 4) < 2;
                        if (show) {
                            draw_digit(x, 1, digit);
                        }
                    } else {
                        draw_digit(x, 1, digit);
                    }
                }
            } else {
                // All digits visible, gentle pulse on border
                for (int i = 0; i < num_digits; i++) {
                    int digit = score_text[i] - '0';
                    draw_digit(i * 4, 1, digit);
                }
                
                // Pulse border
                bool show_border = (animation_frame % 20) < 10;
                if (show_border) {
                    for (int x = 0; x < DISPLAY_WIDTH; x++) {
                        framebuffer[x][7] = 1;  // Top
                        framebuffer[x][6] = 1;  // Bottom
                    }
                }
            }
            break;
        }
        
        case DISPLAY_ANIM_ATTRACT_PINBALL: {
            // Attract-mode loop
            // Scroll "PINBALL" from right to left
            
            const char* text = "PINBALL";
            
            // Calculate text width
            const int CHAR_WIDTH = 4;
            const int CHAR_SPACING = 1;
            int len = (int)strlen(text);
            int text_width = len * (CHAR_WIDTH + CHAR_SPACING) - CHAR_SPACING;
            
            // Loop duration for one complete scroll
            uint32_t loop_duration = 5000;
            uint32_t loop_time = elapsed_ms % loop_duration;
            
            // Brief blink between cycles
            if (loop_time < 200) {
                // Blink phase
                bool show = (animation_frame % 6) < 3;
                if (show) {
                    for (int x = 0; x < DISPLAY_WIDTH; x++) {
                        for (int y = 0; y < DISPLAY_HEIGHT; y++) {
                            framebuffer[x][y] = 1;
                        }
                    }
                }
            } else {
                // Scroll phase
                uint32_t scroll_time = loop_time - 200;
                uint32_t scroll_duration = loop_duration - 200;
                
                int scroll_start = DISPLAY_WIDTH;
                int scroll_end = -text_width;
                int scroll_range = scroll_start - scroll_end;
                
                int scroll_offset = scroll_start - 
                    (int)((scroll_range * scroll_time) / scroll_duration);
                
                display_clear();
                display_set_text(text, scroll_offset, 2);
            }
            break;
        }
        
        case DISPLAY_ANIM_NONE:
        default:
            break;
    }
}