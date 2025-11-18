#ifndef HW_BUTTON_LEDS_H
#define HW_BUTTON_LEDS_H

#include <stdint.h>

// Phase 2 - Button LED control via Adafruit QT board
// TODO: Implement in Phase 2

// Initialize button LED hardware
void hw_button_leds_init(void);

// Set button LED mode
// idx: 0-3 (button index)
// mode: LED mode/pattern
// params: mode-specific parameters
void hw_button_leds_set(uint8_t idx, uint8_t mode, uint8_t *params, uint8_t param_count);

// Update button LEDs (call from main loop)
void hw_button_leds_update(void);

#endif // HW_BUTTON_LEDS_H
