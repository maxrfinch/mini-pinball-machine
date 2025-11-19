#include "hw_button_leds.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <math.h>

// Adafruit NeoKey 1x4 QT I2C Stemma board
// Default I2C address: 0x30
#define NEOKEY_I2C_ADDR     0x30
#define NEOKEY_REG_NEOPIX   0x0E  // NeoPixel color register base

// I2C configuration (shared with buttons)
#define I2C_PORT i2c0

// LED state structure
typedef struct {
    LEDMode mode;
    uint8_t r, g, b;            // Target color
    uint8_t current_r, current_g, current_b;  // Current color for animations
    uint8_t count;              // For blink/strobe: number of flashes
    uint16_t frame;             // Animation frame counter
    uint8_t cycle_count;        // Tracks completed cycles for blink/strobe
} LEDState;

static LEDState led_states[3];  // Left, Center, Right

// Animation parameters
#define BREATHE_PERIOD 120      // Frames for full breathe cycle (2 seconds at 60fps)
#define BLINK_ON_FRAMES 15      // Frames LED is on during blink
#define BLINK_OFF_FRAMES 15     // Frames LED is off during blink
#define STROBE_ON_FRAMES 3      // Frames LED is on during strobe
#define STROBE_OFF_FRAMES 3     // Frames LED is off during strobe

void hw_button_leds_init(void) {
    // Initialize LED states
    for (int i = 0; i < 3; i++) {
        led_states[i].mode = LED_MODE_OFF;
        led_states[i].r = 0;
        led_states[i].g = 0;
        led_states[i].b = 0;
        led_states[i].current_r = 0;
        led_states[i].current_g = 0;
        led_states[i].current_b = 0;
        led_states[i].count = 0;
        led_states[i].frame = 0;
        led_states[i].cycle_count = 0;
    }
    
    // Turn off all LEDs initially
    for (int i = 0; i < 3; i++) {
        hw_button_leds_set(i, LED_MODE_OFF, 0, 0, 0, 0);
    }
}

// Helper function to write RGB color to NeoKey LED
static void write_led_color(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    if (idx > 2) return;  // Only 3 buttons used
    
    // NeoKey uses GRB order for NeoPixels
    uint8_t data[4];
    data[0] = NEOKEY_REG_NEOPIX + (idx * 3);  // Register address for this LED
    data[1] = g;  // Green
    data[2] = r;  // Red
    data[3] = b;  // Blue
    
    i2c_write_blocking(I2C_PORT, NEOKEY_I2C_ADDR, data, 4, false);
}

void hw_button_leds_set(uint8_t idx, LEDMode mode, uint8_t r, uint8_t g, uint8_t b, uint8_t count) {
    if (idx > 2) return;
    
    led_states[idx].mode = mode;
    led_states[idx].r = r;
    led_states[idx].g = g;
    led_states[idx].b = b;
    led_states[idx].count = (count == 0) ? 1 : count;  // Default to 1 if not specified
    led_states[idx].frame = 0;
    led_states[idx].cycle_count = 0;
    
    // For steady mode, immediately set the color
    if (mode == LED_MODE_STEADY) {
        led_states[idx].current_r = r;
        led_states[idx].current_g = g;
        led_states[idx].current_b = b;
        write_led_color(idx, r, g, b);
    }
    // For off mode, immediately turn off
    else if (mode == LED_MODE_OFF) {
        led_states[idx].current_r = 0;
        led_states[idx].current_g = 0;
        led_states[idx].current_b = 0;
        write_led_color(idx, 0, 0, 0);
    }
}

void hw_button_leds_update(void) {
    for (int i = 0; i < 3; i++) {
        LEDState *state = &led_states[i];
        
        switch (state->mode) {
            case LED_MODE_OFF:
                // Nothing to do
                break;
                
            case LED_MODE_STEADY:
                // Already set in hw_button_leds_set
                break;
                
            case LED_MODE_BREATHE: {
                // Sine wave breathing effect
                float angle = (state->frame * 2.0f * 3.14159265f) / BREATHE_PERIOD;
                float brightness = (sinf(angle) + 1.0f) / 2.0f;  // 0.0 to 1.0
                
                state->current_r = (uint8_t)(state->r * brightness);
                state->current_g = (uint8_t)(state->g * brightness);
                state->current_b = (uint8_t)(state->b * brightness);
                
                write_led_color(i, state->current_r, state->current_g, state->current_b);
                
                state->frame++;
                if (state->frame >= BREATHE_PERIOD) {
                    state->frame = 0;
                }
                break;
            }
            
            case LED_MODE_BLINK: {
                uint16_t cycle_length = BLINK_ON_FRAMES + BLINK_OFF_FRAMES;
                uint16_t pos_in_cycle = state->frame % cycle_length;
                
                if (pos_in_cycle < BLINK_ON_FRAMES) {
                    // LED on
                    if (state->current_r == 0 && state->current_g == 0 && state->current_b == 0) {
                        state->current_r = state->r;
                        state->current_g = state->g;
                        state->current_b = state->b;
                        write_led_color(i, state->current_r, state->current_g, state->current_b);
                    }
                } else {
                    // LED off
                    if (state->current_r != 0 || state->current_g != 0 || state->current_b != 0) {
                        state->current_r = 0;
                        state->current_g = 0;
                        state->current_b = 0;
                        write_led_color(i, 0, 0, 0);
                        state->cycle_count++;
                    }
                }
                
                state->frame++;
                
                // Check if we've completed all blinks
                if (state->cycle_count >= state->count) {
                    state->mode = LED_MODE_OFF;
                    state->frame = 0;
                    state->cycle_count = 0;
                    write_led_color(i, 0, 0, 0);
                }
                break;
            }
            
            case LED_MODE_STROBE: {
                uint16_t cycle_length = STROBE_ON_FRAMES + STROBE_OFF_FRAMES;
                uint16_t pos_in_cycle = state->frame % cycle_length;
                
                if (pos_in_cycle < STROBE_ON_FRAMES) {
                    // LED on
                    if (state->current_r == 0 && state->current_g == 0 && state->current_b == 0) {
                        state->current_r = state->r;
                        state->current_g = state->g;
                        state->current_b = state->b;
                        write_led_color(i, state->current_r, state->current_g, state->current_b);
                    }
                } else {
                    // LED off
                    if (state->current_r != 0 || state->current_g != 0 || state->current_b != 0) {
                        state->current_r = 0;
                        state->current_g = 0;
                        state->current_b = 0;
                        write_led_color(i, 0, 0, 0);
                        state->cycle_count++;
                    }
                }
                
                state->frame++;
                
                // Check if we've completed all strobes
                if (state->cycle_count >= state->count) {
                    state->mode = LED_MODE_OFF;
                    state->frame = 0;
                    state->cycle_count = 0;
                    write_led_color(i, 0, 0, 0);
                }
                break;
            }
        }
    }
}
