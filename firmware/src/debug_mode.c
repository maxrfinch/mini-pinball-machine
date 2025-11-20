/**
 * debug_mode.c
 * 
 * Debug mode implementation
 */

#include "pico/stdlib.h"
#include "debug_mode.h"
#include "protocol.h"
#include "neopixel.h"
#include "buttons.h"
#include "haptics.h"
#include "display.h"
#include "types.h"

static bool debug_active = false;
static uint32_t debug_frame = 0;
static absolute_time_t last_debug_update = 0;
static absolute_time_t last_haptic_buzz = 0;

void debug_mode_init(void) {
    debug_active = false;
    debug_frame = 0;
}

void debug_mode_check(void) {
    if (protocol_is_debug_timeout() && !debug_active) {
        // Enter debug mode
        debug_active = true;
        debug_frame = 0;
        protocol_send_debug_active();
        last_haptic_buzz = get_absolute_time();
    }
}

void debug_mode_exit(void) {
    if (debug_active) {
        debug_active = false;
        debug_frame = 0;
        
        // Clear everything
        neopixel_clear();
        neopixel_show();
        display_clear();
        display_update();
    }
}

bool debug_mode_is_active(void) {
    return debug_active;
}

static void debug_neopixels(void) {
    // Board-ID chase, global rainbow, reverse-direction test
    neopixel_clear();
    
    uint8_t mode = (debug_frame / 100) % 3;
    
    if (mode == 0) {
        // Board-ID chase - highlight each board sequentially
        uint8_t board = (debug_frame / 20) % 6;
        Color colors[6] = {
            {255, 0, 0},    // Red
            {0, 255, 0},    // Green
            {0, 0, 255},    // Blue
            {255, 255, 0},  // Yellow
            {255, 0, 255},  // Magenta
            {0, 255, 255}   // Cyan
        };
        
        const LedBoard* b = neopixel_get_board(board + 1);
        if (b) {
            for (int i = b->start_index; i <= b->end_index; i++) {
                neopixel_set_led(i, colors[board]);
            }
        }
        
    } else if (mode == 1) {
        // Global rainbow
        for (int i = 0; i < 48; i++) {
            uint8_t hue = (debug_frame * 2 + i * 5) % 256;
            uint8_t r = (hue < 85) ? (255 - hue * 3) : (hue < 170) ? 0 : (hue - 170) * 3;
            uint8_t g = (hue < 85) ? hue * 3 : (hue < 170) ? (255 - (hue - 85) * 3) : 0;
            uint8_t b = (hue < 85) ? 0 : (hue < 170) ? (hue - 85) * 3 : (255 - (hue - 170) * 3);
            Color c = {r, g, b};
            neopixel_set_led(i, c);
        }
        
    } else {
        // Reverse-direction test - alternate forward/backward sweep
        uint8_t pos = debug_frame % 48;
        Color white = {255, 255, 255};
        neopixel_set_led(pos, white);
        
        // Also show reverse on opposite end
        neopixel_set_led(47 - pos, white);
    }
    
    neopixel_show();
}

static void debug_button_leds(void) {
    // Left: slow pulse, Center: steady, Right: fast pulse
    uint8_t slow_pulse = (debug_frame / 10) % 2 ? 255 : 0;
    uint8_t fast_pulse = (debug_frame / 3) % 2 ? 255 : 0;
    
    buttons_set_led(BUTTON_LEFT, slow_pulse);
    buttons_set_led(BUTTON_CENTER, 255);
    buttons_set_led(BUTTON_RIGHT, fast_pulse);
}

static void debug_displays(void) {
    // Use the test pattern function
    display_test_pattern();
}

static void debug_haptics_periodic(void) {
    // Light buzz every 10 seconds
    absolute_time_t now = get_absolute_time();
    int64_t diff_ms = absolute_time_diff_us(last_haptic_buzz, now) / 1000;
    
    if (diff_ms > 10000) {
        haptics_light_buzz();
        last_haptic_buzz = now;
    }
}

void debug_mode_update(void) {
    absolute_time_t now = get_absolute_time();
    int64_t diff_ms = absolute_time_diff_us(last_debug_update, now) / 1000;
    
    if (diff_ms < 16) { // ~60 FPS
        return;
    }
    
    last_debug_update = now;
    debug_frame++;
    
    // Update debug animations
    debug_neopixels();
    debug_button_leds();
    debug_displays();
    debug_haptics_periodic();
}
