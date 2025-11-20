/**
 * neopixel.c
 * 
 * NeoPixel LED control driver with effects
 */

#include <math.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "neopixel.h"
#include "hardware_config.h"

// PIO program for WS2812
#include "ws2812.pio.h"

static PIO pio = pio0;
static uint sm = 0;
static Color led_buffer[NEOPIXEL_COUNT];
static uint8_t brightness = 255;
static LedEffect current_effect = EFFECT_NONE;
static uint32_t effect_frame = 0;
static absolute_time_t last_effect_update = 0;

// LED Board configuration
static const LedBoard boards[NEOPIXEL_BOARDS] = {
    {1, 0, 7, false, BOARD_POS_RIGHT_FRONT},   // Board 1: LEDs 0-7
    {2, 8, 15, false, BOARD_POS_RIGHT_REAR},   // Board 2: LEDs 8-15
    {3, 16, 23, false, BOARD_POS_CAMERA_BAR},  // Board 3: LEDs 16-23
    {4, 24, 31, true, BOARD_POS_LEFT_REAR},    // Board 4: LEDs 24-31 (reversed)
    {5, 32, 39, true, BOARD_POS_LEFT_FRONT},   // Board 5: LEDs 32-39 (reversed)
    {6, 40, 47, false, BOARD_POS_FRONT_BAR}    // Board 6: LEDs 40-47
};

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t r_scaled = (r * brightness) / 255;
    uint8_t g_scaled = (g * brightness) / 255;
    uint8_t b_scaled = (b * brightness) / 255;
    return ((uint32_t)r_scaled << 8) | ((uint32_t)g_scaled << 16) | (uint32_t)b_scaled;
}

// HSV to RGB conversion
static Color hsv_to_rgb(uint16_t h, uint8_t s, uint8_t v) {
    uint8_t region, remainder, p, q, t;
    Color rgb;
    
    if (s == 0) {
        rgb.r = rgb.g = rgb.b = v;
        return rgb;
    }
    
    region = h / 43;
    remainder = (h - (region * 43)) * 6;
    
    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region) {
        case 0: rgb.r = v; rgb.g = t; rgb.b = p; break;
        case 1: rgb.r = q; rgb.g = v; rgb.b = p; break;
        case 2: rgb.r = p; rgb.g = v; rgb.b = t; break;
        case 3: rgb.r = p; rgb.g = q; rgb.b = v; break;
        case 4: rgb.r = t; rgb.g = p; rgb.b = v; break;
        default: rgb.r = v; rgb.g = p; rgb.b = q; break;
    }
    
    return rgb;
}

void neopixel_init(void) {
    // Load PIO program
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, NEOPIXEL_PIN, 800000, false);
    
    // Clear all LEDs
    memset(led_buffer, 0, sizeof(led_buffer));
    neopixel_show();
}

void neopixel_set_brightness(uint8_t b) {
    brightness = b;
}

void neopixel_set_led(uint8_t index, Color color) {
    if (index < NEOPIXEL_COUNT) {
        led_buffer[index] = color;
    }
}

void neopixel_fill(Color color) {
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        led_buffer[i] = color;
    }
}

void neopixel_clear(void) {
    Color black = {0, 0, 0};
    neopixel_fill(black);
}

void neopixel_show(void) {
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        put_pixel(urgb_u32(led_buffer[i].r, led_buffer[i].g, led_buffer[i].b));
    }
}

void neopixel_start_effect(LedEffect effect) {
    current_effect = effect;
    effect_frame = 0;
    last_effect_update = get_absolute_time();
}

const LedBoard* neopixel_get_board(uint8_t board_id) {
    if (board_id >= 1 && board_id <= NEOPIXEL_BOARDS) {
        return &boards[board_id - 1];
    }
    return NULL;
}

// Effect implementations
static void effect_rainbow_breathe(void) {
    // Slow rainbow with brightness oscillation
    uint8_t breathe = (uint8_t)(127 + 127 * sin(effect_frame * 0.02));
    uint16_t hue = (effect_frame * 2) % 256;
    
    Color color = hsv_to_rgb(hue, 255, breathe);
    neopixel_fill(color);
}

static void effect_rainbow_wave(void) {
    // Full-cabinet rainbow wave
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        uint16_t hue = ((effect_frame * 4 + i * 256 / NEOPIXEL_COUNT) % 256);
        Color color = hsv_to_rgb(hue, 255, 255);
        neopixel_set_led(i, color);
    }
}

static void effect_camera_flash(void) {
    // Only Board #3 flashes white, rest off
    neopixel_clear();
    
    if ((effect_frame / 5) % 2 == 0) {
        Color white = {255, 255, 255};
        for (int i = 16; i <= 23; i++) {
            neopixel_set_led(i, white);
        }
    }
}

static void effect_red_strobe_5x(void) {
    // Red strobe (5 flashes)
    Color red = {255, 0, 0};
    Color black = {0, 0, 0};
    
    uint32_t flash_num = effect_frame / 5;
    if (flash_num >= 5) {
        neopixel_clear();
        return;
    }
    
    if ((effect_frame % 5) < 2) {
        neopixel_fill(red);
    } else {
        neopixel_fill(black);
    }
}

static void effect_water(void) {
    // Aqua "flow" animation
    for (int i = 0; i < NEOPIXEL_COUNT; i++) {
        uint8_t wave = (uint8_t)(127 + 127 * sin((effect_frame * 0.1) + (i * 0.3)));
        Color aqua = {0, wave, wave / 2};
        neopixel_set_led(i, aqua);
    }
}

static void effect_attract(void) {
    // Idle attract-mode pattern - rainbow chase
    neopixel_clear();
    uint8_t pos = effect_frame % NEOPIXEL_COUNT;
    uint16_t hue = (effect_frame * 8) % 256;
    Color color = hsv_to_rgb(hue, 255, 255);
    
    for (int i = 0; i < 3; i++) {
        uint8_t idx = (pos + i) % NEOPIXEL_COUNT;
        neopixel_set_led(idx, color);
    }
}

static void effect_pink_pulse(void) {
    // Sharp pink pulse
    uint8_t pulse = (effect_frame % 20) < 10 ? 255 : 0;
    Color pink = {255, pulse / 2, pulse};
    neopixel_fill(pink);
}

static void effect_ball_launch(void) {
    // Left/right walls ripple front→back (yellow)
    Color yellow = {255, 255, 0};
    Color black = {0, 0, 0};
    neopixel_clear();
    
    uint32_t wave_pos = effect_frame % 8;
    
    // Right wall (boards 1, 2)
    for (int i = 0; i < 16; i++) {
        if ((i % 8) == wave_pos) {
            neopixel_set_led(i, yellow);
        }
    }
    
    // Left wall (boards 4, 5)
    for (int i = 24; i < 40; i++) {
        if ((i % 8) == wave_pos) {
            neopixel_set_led(i, yellow);
        }
    }
}

void neopixel_update_effect(void) {
    absolute_time_t now = get_absolute_time();
    int64_t diff_ms = absolute_time_diff_us(last_effect_update, now) / 1000;
    
    if (diff_ms < 16) { // ~60 FPS
        return;
    }
    
    last_effect_update = now;
    effect_frame++;
    
    switch (current_effect) {
        case EFFECT_RAINBOW_BREATHE:
            effect_rainbow_breathe();
            break;
        case EFFECT_RAINBOW_WAVE:
            effect_rainbow_wave();
            break;
        case EFFECT_CAMERA_FLASH:
            effect_camera_flash();
            break;
        case EFFECT_RED_STROBE_5X:
            effect_red_strobe_5x();
            break;
        case EFFECT_WATER:
            effect_water();
            break;
        case EFFECT_ATTRACT:
            effect_attract();
            break;
        case EFFECT_PINK_PULSE:
            effect_pink_pulse();
            break;
        case EFFECT_BALL_LAUNCH:
            effect_ball_launch();
            break;
        case EFFECT_NONE:
        default:
            return;
    }
    
    neopixel_show();
}
