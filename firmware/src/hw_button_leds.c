#include "hw_button_leds.h"
#include "hw_buttons.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// -----------------------------------------------------------------------------
// Internal representation of each button LED
// -----------------------------------------------------------------------------

typedef struct {
    LEDMode mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    // For BLINK / STROBE: how many full on+off cycles remain (0 = none)
    uint8_t remaining;

    bool is_on;
    absolute_time_t last_toggle;
    
    // For BREATHE animation: phase tracking (0-255)
    uint8_t breathe_phase;
    bool breathe_rising;
} ButtonLEDState;

static ButtonLEDState g_leds[3];

// Baseline LED patterns for each button
typedef struct {
    LEDMode mode;
    uint8_t r, g, b;
} ButtonLedBaseline;

static ButtonLedBaseline g_baseline[3];
static LedGameState g_game_state = LED_GAME_STATE_MENU;
static bool g_ball_ready = false;

// White LED constants
static const uint8_t LED_WHITE_R = 255;
static const uint8_t LED_WHITE_G = 255;
static const uint8_t LED_WHITE_B = 255;

// Timing (ms) for different modes
#define BLINK_INTERVAL_MS        150u
#define STROBE_INTERVAL_MS        60u
#define RAPID_BLINK_INTERVAL_MS  100u
#define BREATHE_FAST_STEP_MS      15u  // Fast breathe cycle for menu

// -----------------------------------------------------------------------------
// Hardware hook – implement actual LED driving via Arcade 1x4 QT (seesaw)
// -----------------------------------------------------------------------------

// Match I2C configuration used in hw_buttons.c
#define ARCADEQT_I2C_ADDR  0x3A
#define I2C_PORT           i2c0

// Seesaw PWM module base and value register
#define SEESAW_PWM_BASE    0x08
#define SEESAW_PWM_VAL     0x01

// Map our logical button indices to Arcade QT LED pins.
// Adjust mapping if your physical wiring uses different LED positions.
static uint8_t led_idx_to_pwm_pin(uint8_t idx) {
    switch (idx) {
        case BUTTON_LED_LEFT:
            // LED1 on Arcade QT
            return 12;
        case BUTTON_LED_CENTER:
            // LED2 on Arcade QT
            return 13;
        case BUTTON_LED_RIGHT:
            // LED3 on Arcade QT
            return 0;
        default:
            return 0xFF; // invalid
    }
}

// Low-level helper: write a 16-bit PWM value to a given seesaw pin
static void arcadeqt_pwm_write(uint8_t pwm_pin, uint16_t value) {
    uint8_t buf[5];
    buf[0] = SEESAW_PWM_BASE;   // module base
    buf[1] = SEESAW_PWM_VAL;    // PWM value register
    buf[2] = pwm_pin;           // which PWM pin
    buf[3] = (uint8_t)(value >> 8);
    buf[4] = (uint8_t)(value & 0xFF);

    // Best-effort write; ignore error codes for now
    i2c_write_blocking(I2C_PORT, ARCADEQT_I2C_ADDR, buf, 5, false);
}

// Drive a single LED according to our logical state
static void apply_led(uint8_t idx, bool on, uint8_t r, uint8_t g, uint8_t b) {
    uint8_t pwm_pin = led_idx_to_pwm_pin(idx);
    if (pwm_pin == 0xFF) {
        return; // Unknown LED index
    }

    // For single-color arcade button LEDs, treat RGB as brightness and use
    // the maximum of R/G/B as the effective brightness value.
    uint8_t brightness8 = 0;
    if (on) {
        uint8_t max1 = (r > g) ? r : g;
        brightness8 = (max1 > b) ? max1 : b;
    }

    // Scale 8-bit 0–255 to 16-bit 0–65535 for seesaw PWM
    uint16_t pwm_val = ((uint16_t)brightness8) << 8;

    arcadeqt_pwm_write(pwm_pin, pwm_val);
}

// -----------------------------------------------------------------------------
// Baseline pattern management
// -----------------------------------------------------------------------------