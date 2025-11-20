#ifndef HW_BUTTONS_H
#define HW_BUTTONS_H

#include <stdint.h>

// Logical button bits (match what you actually see on the wire):
//   left   -> 0x01 (bit 0)
//   center -> 0x02 (bit 1)
//   right  -> 0x04 (bit 2)
#define BUTTON_LEFT_BIT      0
#define BUTTON_CENTER_BIT    1
#define BUTTON_RIGHT_BIT     2

#define BUTTON_LEFT_MASK    (1u << BUTTON_LEFT_BIT)    // 0x01
#define BUTTON_CENTER_MASK  (1u << BUTTON_CENTER_BIT)  // 0x02
#define BUTTON_RIGHT_MASK   (1u << BUTTON_RIGHT_BIT)   // 0x04

// Initialize button hardware (Adafruit QT I2C board)
void hw_buttons_init(void);

// Poll button states and return as byte
// bit layout: LEFT=0, CENTER=1, RIGHT=2
uint8_t hw_buttons_poll(void);

// Get current button state byte
uint8_t hw_buttons_get_state(void);

#endif // HW_BUTTONS_H