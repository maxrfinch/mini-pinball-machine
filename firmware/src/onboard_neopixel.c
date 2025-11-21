/**
 * onboard_neopixel.c
 * 
 * KB2040 onboard NeoPixel control with cycling animations
 */

#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "onboard_neopixel.h"

// PIO program for WS2812
#include "ws2812.pio.h"

#define ONBOARD_NEOPIXEL_PIN 17
#define ANIMATION_SPEED_MS 16  // ~60 FPS

static PIO pio = pio1;  // Use PIO1 to avoid conflict with external NeoPixels on PIO0
static uint sm = 0;
static uint8_t brightness = 128;  // Default medium brightness
static bool debug_mode = false;
static uint32_t animation_frame = 0;
static absolute_time_t last_update = 0;

// Animation states
typedef enum {
    ANIM_RAINBOW_CYCLE = 0,
    ANIM_PULSE_BLUE,
    ANIM_PULSE_PURPLE,
    ANIM_FIRE,
    ANIM_OCEAN,
    ANIM_CANDY,
    ANIM_COUNT
} AnimationMode;

static AnimationMode current_animation = ANIM_RAINBOW_CYCLE;
static uint32_t animation_mode_frame = 0;
static const uint32_t FRAMES_PER_ANIMATION = 600;  // ~10 seconds per animation at 60 FPS

// HSL to RGB conversion
static Color hsl_to_rgb(float h, float s, float l) {
    Color rgb;
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;
    
    float r_temp, g_temp, b_temp;
    
    if (h >= 0.0f && h < 60.0f) {
        r_temp = c; g_temp = x; b_temp = 0.0f;
    } else if (h >= 60.0f && h < 120.0f) {
        r_temp = x; g_temp = c; b_temp = 0.0f;
    } else if (h >= 120.0f && h < 180.0f) {
        r_temp = 0.0f; g_temp = c; b_temp = x;
    } else if (h >= 180.0f && h < 240.0f) {
        r_temp = 0.0f; g_temp = x; b_temp = c;
    } else if (h >= 240.0f && h < 300.0f) {
        r_temp = x; g_temp = 0.0f; b_temp = c;
    } else {
        r_temp = c; g_temp = 0.0f; b_temp = x;
    }
    
    rgb.r = (uint8_t)((r_temp + m) * 255.0f);
    rgb.g = (uint8_t)((g_temp + m) * 255.0f);
    rgb.b = (uint8_t)((b_temp + m) * 255.0f);
    
    return rgb;
}

// HSV to RGB conversion (simpler for some effects)
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

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t r_scaled = (r * brightness) / 255;
    uint8_t g_scaled = (g * brightness) / 255;
    uint8_t b_scaled = (b * brightness) / 255;
    return ((uint32_t)r_scaled << 8) | ((uint32_t)g_scaled << 16) | (uint32_t)b_scaled;
}

static void set_color(Color color) {
    put_pixel(urgb_u32(color.r, color.g, color.b));
}

// Animation: Rainbow cycle
static void animate_rainbow_cycle(void) {
    uint16_t hue = (animation_frame * 2) % 256;
    Color color = hsv_to_rgb(hue, 255, 255);
    set_color(color);
}

// Animation: Blue pulse
static void animate_pulse_blue(void) {
    float pulse = 0.5f + 0.5f * sinf(animation_frame * 0.05f);
    Color color = {0, (uint8_t)(pulse * 128), (uint8_t)(pulse * 255)};
    set_color(color);
}

// Animation: Purple pulse
static void animate_pulse_purple(void) {
    float pulse = 0.5f + 0.5f * sinf(animation_frame * 0.04f);
    Color color = {(uint8_t)(pulse * 200), 0, (uint8_t)(pulse * 255)};
    set_color(color);
}

// Animation: Fire effect (red-orange-yellow)
static void animate_fire(void) {
    float flicker = 0.7f + 0.3f * sinf(animation_frame * 0.3f) * cosf(animation_frame * 0.17f);
    uint8_t red = (uint8_t)(255 * flicker);
    uint8_t green = (uint8_t)(100 * flicker);
    uint8_t blue = 0;
    Color color = {red, green, blue};
    set_color(color);
}

// Animation: Ocean wave (cyan-blue)
static void animate_ocean(void) {
    float wave = 0.5f + 0.5f * sinf(animation_frame * 0.06f);
    Color color = {0, (uint8_t)(wave * 180), (uint8_t)(255 * wave)};
    set_color(color);
}

// Animation: Candy (pink-white transition)
static void animate_candy(void) {
    float transition = 0.5f + 0.5f * sinf(animation_frame * 0.03f);
    uint8_t white_amt = (uint8_t)(transition * 100);
    Color color = {255, white_amt, (uint8_t)(200 + white_amt * 0.27f)};
    set_color(color);
}

// Debug mode: Steady orange using HSL
static void set_debug_orange(void) {
    // Orange in HSL: H=30°, S=100%, L=50%
    Color orange = hsl_to_rgb(30.0f, 1.0f, 0.5f);
    set_color(orange);
}

void onboard_neopixel_init(void) {
    // Initialize PIO1 for onboard NeoPixel
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, ONBOARD_NEOPIXEL_PIN, 800000, false);
    
    // Set initial color to off
    Color black = {0, 0, 0};
    set_color(black);
    
    last_update = get_absolute_time();
}

void onboard_neopixel_update(void) {
    absolute_time_t now = get_absolute_time();
    int64_t diff_ms = absolute_time_diff_us(last_update, now) / 1000;
    
    if (diff_ms < ANIMATION_SPEED_MS) {
        return;
    }
    
    last_update = now;
    
    // In debug mode, show steady orange
    if (debug_mode) {
        set_debug_orange();
        return;
    }
    
    // Cycle through animations
    animation_frame++;
    animation_mode_frame++;
    
    if (animation_mode_frame >= FRAMES_PER_ANIMATION) {
        animation_mode_frame = 0;
        current_animation = (current_animation + 1) % ANIM_COUNT;
    }
    
    // Execute current animation
    switch (current_animation) {
        case ANIM_RAINBOW_CYCLE:
            animate_rainbow_cycle();
            break;
        case ANIM_PULSE_BLUE:
            animate_pulse_blue();
            break;
        case ANIM_PULSE_PURPLE:
            animate_pulse_purple();
            break;
        case ANIM_FIRE:
            animate_fire();
            break;
        case ANIM_OCEAN:
            animate_ocean();
            break;
        case ANIM_CANDY:
            animate_candy();
            break;
        default:
            animate_rainbow_cycle();
            break;
    }
}

void onboard_neopixel_set_debug_mode(bool enabled) {
    debug_mode = enabled;
    if (enabled) {
        animation_frame = 0;
        animation_mode_frame = 0;
    }
}

void onboard_neopixel_set_brightness(uint8_t b) {
    brightness = b;
}
