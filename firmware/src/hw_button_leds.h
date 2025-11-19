#ifndef HW_BUTTON_LEDS_H
#define HW_BUTTON_LEDS_H

#include <stdint.h>
#include <stdbool.h>

// Button LED control via Adafruit NeoKey QT board

// LED animation modes
typedef enum {
    LED_MODE_OFF = 0,       // LED off
    LED_MODE_STEADY = 1,    // Solid color
    LED_MODE_BREATHE = 2,   // Breathing animation
    LED_MODE_BLINK = 3,     // Blink N times
    LED_MODE_STROBE = 4,    // Fast strobe N times
    LED_MODE_RAPID_BLINK = 5 // Continuous rapid blink (ball ready)
} LEDMode;

// Button indices (matching physical layout)
#define BUTTON_LED_LEFT     0
#define BUTTON_LED_CENTER   1
#define BUTTON_LED_RIGHT    2

// LED game states
typedef enum {
    LED_GAME_STATE_MENU = 0,
    LED_GAME_STATE_IN_GAME = 1,
    LED_GAME_STATE_GAME_OVER = 2
} LedGameState;

// Initialize button LED hardware
void hw_button_leds_init(void);

// Set button LED mode with color
// idx: button index (0=left, 1=center, 2=right)
// mode: LED mode (off, steady, breathe, blink, strobe)
// r, g, b: color values (0-255)
// count: for blink/strobe modes, number of times to flash (default 1)
void hw_button_leds_set(uint8_t idx,
                        LEDMode mode,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b,
                        uint8_t count);

// Update button LEDs (call from main loop)
void hw_button_leds_update(void);

// Convenience helpers for dev/debug patterns
void hw_button_led_blink_left(uint8_t times);
void hw_button_led_blink_center(uint8_t times);
void hw_button_led_blink_right(uint8_t times);

// Game-state API
void button_leds_set_game_state(LedGameState state);
void button_leds_on_game_start(void);
void button_leds_on_ball_ready(void);
void button_leds_on_ball_launched(void);
void button_leds_on_button_pressed(uint8_t button_state, uint8_t pressed_bits);

#endif // HW_BUTTON_LEDS_H