#include "hw_neopixel.h"

// Phase 2 - NeoPixel control
// TODO: Implement in Phase 2

void hw_neopixel_init(void) {
    // TODO: Initialize NeoPixel strip (65 LEDs)
    // Use PIO state machine for WS2812B protocol
}

void hw_neopixel_set(uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
    // TODO: Set individual LED color
    (void)idx;
    (void)r;
    (void)g;
    (void)b;
}

void hw_neopixel_fill(uint8_t r, uint8_t g, uint8_t b) {
    // TODO: Fill all LEDs with same color
    (void)r;
    (void)g;
    (void)b;
}

void hw_neopixel_mode(uint8_t mode_id, uint8_t *params, uint8_t param_count) {
    // TODO: Set animation mode
    (void)mode_id;
    (void)params;
    (void)param_count;
}

void hw_neopixel_power_mode(uint8_t mode_id) {
    // TODO: Control power LED (LED 64)
    (void)mode_id;
}

void hw_neopixel_update(void) {
    // TODO: Update animation states, refresh LEDs
}
