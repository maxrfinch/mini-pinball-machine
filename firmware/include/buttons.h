/**
 * buttons.h
 * 
 * Adafruit Seesaw button driver
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include "types.h"

// Initialize button subsystem
void buttons_init(void);

// Poll button states (call periodically)
void buttons_poll(void);

// Set button LED state (for debug mode)
void buttons_set_led(Button button, uint8_t brightness);

// Set button LED pulse mode
void buttons_set_led_pulse(Button button, bool slow);

// Get current button state
bool buttons_is_pressed(Button button);

#endif // BUTTONS_H
