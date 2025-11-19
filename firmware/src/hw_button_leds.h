#ifndef HW_BUTTON_LEDS_H
#define HW_BUTTON_LEDS_H

#include <stdint.h>

// Button LED control via Adafruit NeoKey QT board

// LED animation modes
typedef enum {
    LED_MODE_OFF = 0,       // LED off
    LED_MODE_STEADY = 1,    // Solid color
    LED_MODE_BREATHE = 2,   // Breathing animation
    LED_MODE_BLINK = 3,     // Blink N times
    LED_MODE_STROBE = 4     // Fast strobe N times
} LEDMode;

// Button indices (matching physical layout)
#define BUTTON_LED_LEFT     0
#define BUTTON_LED_CENTER   1
#define BUTTON_LED_RIGHT    2

// Initialize button LED hardware
void hw_button_leds_init(void);

// Set button LED mode with color
// idx: button index (0=left, 1=center, 2=right)
// mode: LED mode (off, steady, breathe, blink, strobe)
// r, g, b: color values (0-255)
// count: for blink/strobe modes, number of times to flash (default 1)
void hw_button_leds_set(uint8_t idx, LEDMode mode, uint8_t r, uint8_t g, uint8_t b, uint8_t count);

// Update button LEDs (call from main loop)
void hw_button_leds_update(void);

#endif // HW_BUTTON_LEDS_H
