#include "hw_button_leds.h"

// Phase 2 - Button LED control
// TODO: Implement in Phase 2

void hw_button_leds_init(void) {
    // TODO: Initialize LED control via NeoKey QT board
    // Use I2C to control onboard NeoPixels
}

void hw_button_leds_set(uint8_t idx, uint8_t mode, uint8_t *params, uint8_t param_count) {
    // TODO: Set button LED mode/color
    (void)idx;
    (void)mode;
    (void)params;
    (void)param_count;
}

void hw_button_leds_update(void) {
    // TODO: Update button LED animations
}
