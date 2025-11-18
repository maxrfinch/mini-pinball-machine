#ifndef HW_BUTTONS_H
#define HW_BUTTONS_H

#include <stdint.h>

// Button bit positions (matching original Arduino protocol)
#define BUTTON_CENTER_BIT   0  // Bit 0
#define BUTTON_RIGHT_BIT    1  // Bit 1
#define BUTTON_LEFT_BIT     2  // Bit 2

// Initialize button hardware (Adafruit QT I2C board)
void hw_buttons_init(void);

// Poll button states and return as byte (bit layout: left=2, right=1, center=0)
uint8_t hw_buttons_poll(void);

// Get current button state byte
uint8_t hw_buttons_get_state(void);

#endif // HW_BUTTONS_H
