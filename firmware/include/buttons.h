/**
 * buttons.h
 * 
 * Adafruit LED Arcade Button 1×4 I2C Breakout (Arcade Seesaw) driver
 */

#ifndef BUTTONS_H
#define BUTTONS_H

#include "types.h"

// Button LED Effect Modes
typedef enum {
    BTN_EFFECT_OFF = 0,              // LEDs off
    BTN_EFFECT_READY_STEADY_GLOW,    // Idle/attract - slow breathing fade
    BTN_EFFECT_FLIPPER_FEEDBACK,     // Button press - fast pop
    BTN_EFFECT_CENTER_HIT_PULSE,     // Launch ready - rapid double strobe
    BTN_EFFECT_SKILL_SHOT_BUILDUP,   // Pre-launch - ramp 0→255
    BTN_EFFECT_BALL_SAVED,           // Multiball/save - alternating flash
    BTN_EFFECT_POWERUP_ALERT,        // Special state - chaotic strobe
    BTN_EFFECT_EXTRA_BALL_AWARD,     // Award - 3 pulses + fade
    BTN_EFFECT_GAME_OVER_FADE,       // Game over - slow fade all
    BTN_EFFECT_MENU_NAVIGATION       // Menu - selection indicators
} ButtonLEDEffect;

// Initialize button subsystem
void buttons_init(void);

// Poll button states (call periodically)
void buttons_poll(void);

// Update button LED effects (call from main loop)
void buttons_update_leds(void);

// Start a button LED effect
void buttons_start_effect(ButtonLEDEffect effect);

// Set button LED state (direct control for debug mode)
void buttons_set_led(Button button, uint8_t brightness);

// Set button LED pulse mode
void buttons_set_led_pulse(Button button, bool slow);

// Get current button state
bool buttons_is_pressed(Button button);

// Set menu selection (for menu navigation effect)
void buttons_set_menu_selection(Button button);

#endif // BUTTONS_H
