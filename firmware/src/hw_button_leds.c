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

static void button_leds_set_baseline(uint8_t idx, LEDMode mode, uint8_t r, uint8_t g, uint8_t b) {
    if (idx > BUTTON_LED_RIGHT) {
        return;
    }
    g_baseline[idx].mode = mode;
    g_baseline[idx].r = r;
    g_baseline[idx].g = g;
    g_baseline[idx].b = b;
}

static void button_leds_apply_baseline(uint8_t idx) {
    if (idx > BUTTON_LED_RIGHT) {
        return;
    }
    hw_button_leds_set(idx,
                       g_baseline[idx].mode,
                       g_baseline[idx].r,
                       g_baseline[idx].g,
                       g_baseline[idx].b,
                       0);  // no extra blink cycles
}

static void button_leds_apply_all_baselines(void) {
    for (uint8_t i = 0; i <= BUTTON_LED_RIGHT; i++) {
        button_leds_apply_baseline(i);
    }
}

static void apply_menu_baseline(void) {
    // Left: steady white
    button_leds_set_baseline(BUTTON_LED_LEFT,
                             LED_MODE_STEADY,
                             LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Center: breathe white
    button_leds_set_baseline(BUTTON_LED_CENTER,
                             LED_MODE_BREATHE,
                             LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Right: steady white
    button_leds_set_baseline(BUTTON_LED_RIGHT,
                             LED_MODE_STEADY,
                             LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);

    button_leds_apply_all_baselines();
}

static void apply_ingame_baseline(void) {
    // Left and right steady white in game
    button_leds_set_baseline(BUTTON_LED_LEFT,
                             LED_MODE_STEADY,
                             LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Center LED off in normal gameplay (will blink when ball ready)
    button_leds_set_baseline(BUTTON_LED_CENTER,
                             LED_MODE_OFF,
                             0, 0, 0);
    button_leds_set_baseline(BUTTON_LED_RIGHT,
                             LED_MODE_STEADY,
                             LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);

    button_leds_apply_all_baselines();
}

static void apply_gameover_baseline(void) {
    // For now, reuse menu pattern on game over
    apply_menu_baseline();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void hw_button_leds_init(void) {
    for (uint8_t i = 0; i < 3; ++i) {
        g_leds[i].mode          = LED_MODE_OFF;
        g_leds[i].r             = 0;
        g_leds[i].g             = 0;
        g_leds[i].b             = 0;
        g_leds[i].remaining     = 0;
        g_leds[i].is_on         = false;
        g_leds[i].last_toggle   = get_absolute_time();
        g_leds[i].breathe_phase = 0;
        g_leds[i].breathe_rising= true;

        apply_led(i, false, 0, 0, 0);
    }

    g_game_state = LED_GAME_STATE_MENU;
    g_ball_ready = false;
    apply_menu_baseline();
}

void hw_button_leds_set(uint8_t idx,
                        LEDMode mode,
                        uint8_t r,
                        uint8_t g,
                        uint8_t b,
                        uint8_t count) {
    if (idx > BUTTON_LED_RIGHT) {
        return;
    }

    ButtonLEDState *s = &g_leds[idx];
    s->mode      = mode;
    s->r         = r;
    s->g         = g;
    s->b         = b;
    s->is_on     = false;
    s->last_toggle = get_absolute_time();

    if (mode == LED_MODE_BLINK || mode == LED_MODE_STROBE) {
        s->remaining = (count == 0) ? 1 : count;  // at least one cycle
    } else {
        s->remaining = 0;
    }

    if (mode == LED_MODE_BREATHE) {
        s->breathe_phase  = 0;
        s->breathe_rising = true;
    }

    switch (mode) {
    case LED_MODE_OFF:
        s->is_on = false;
        apply_led(idx, false, 0, 0, 0);
        break;

    case LED_MODE_STEADY:
        s->is_on = true;
        apply_led(idx, true, s->r, s->g, s->b);
        break;

    case LED_MODE_BREATHE:
        // Start at minimum brightness, update in hw_button_leds_update
        s->is_on = true;
        apply_led(idx, true, 0, 0, 0);
        break;

    case LED_MODE_BLINK:
    case LED_MODE_STROBE:
    case LED_MODE_RAPID_BLINK:
        // Animation handled in update loop
        s->is_on = false;
        apply_led(idx, false, 0, 0, 0);
        break;
    }
}

void hw_button_leds_update(void) {
    absolute_time_t now = get_absolute_time();

    for (uint8_t idx = 0; idx < 3; ++idx) {
        ButtonLEDState *s = &g_leds[idx];

        switch (s->mode) {
        case LED_MODE_OFF:
            if (s->is_on) {
                s->is_on = false;
                apply_led(idx, false, 0, 0, 0);
            }
            break;

        case LED_MODE_STEADY:
            if (!s->is_on) {
                s->is_on = true;
                apply_led(idx, true, s->r, s->g, s->b);
            }
            break;

        case LED_MODE_BREATHE: {
            int64_t elapsed_ms = absolute_time_diff_us(s->last_toggle, now) / 1000;
            if (elapsed_ms >= BREATHE_FAST_STEP_MS) {
                s->last_toggle = now;

                if (s->breathe_rising) {
                    s->breathe_phase += 8;
                    if (s->breathe_phase >= 248) {
                        s->breathe_phase = 255;
                        s->breathe_rising = false;
                    }
                } else {
                    if (s->breathe_phase < 8) {
                        s->breathe_phase = 0;
                        s->breathe_rising = true;
                    } else {
                        s->breathe_phase -= 8;
                    }
                }

                uint8_t rr = (s->r * s->breathe_phase) / 255;
                uint8_t gg = (s->g * s->breathe_phase) / 255;
                uint8_t bb = (s->b * s->breathe_phase) / 255;
                apply_led(idx, true, rr, gg, bb);
            }
            break;
        }

        case LED_MODE_BLINK:
        case LED_MODE_STROBE: {
            if (s->remaining == 0) {
                s->mode = LED_MODE_OFF;
                button_leds_apply_baseline(idx);
                break;
            }

            uint32_t interval_ms =
                (s->mode == LED_MODE_BLINK) ? BLINK_INTERVAL_MS : STROBE_INTERVAL_MS;

            int64_t elapsed_ms = absolute_time_diff_us(s->last_toggle, now) / 1000;
            if (elapsed_ms >= (int64_t)interval_ms) {
                s->last_toggle = now;
                s->is_on = !s->is_on;

                if (s->is_on) {
                    apply_led(idx, true, s->r, s->g, s->b);
                } else {
                    apply_led(idx, false, 0, 0, 0);
                    if (s->remaining > 0) {
                        s->remaining--;
                    }
                    if (s->remaining == 0) {
                        s->mode = LED_MODE_OFF;
                        button_leds_apply_baseline(idx);
                    }
                }
            }
            break;
        }

        case LED_MODE_RAPID_BLINK: {
            int64_t elapsed_ms = absolute_time_diff_us(s->last_toggle, now) / 1000;
            if (elapsed_ms >= RAPID_BLINK_INTERVAL_MS) {
                s->last_toggle = now;
                s->is_on = !s->is_on;

                if (s->is_on) {
                    apply_led(idx, true, s->r, s->g, s->b);
                } else {
                    apply_led(idx, false, 0, 0, 0);
                }
            }
            break;
        }

        default:
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// Convenience blink helpers
// -----------------------------------------------------------------------------

void hw_button_led_blink_left(uint8_t times) {
    if (times == 0) times = 1;
    hw_button_leds_set(BUTTON_LED_LEFT,
                       LED_MODE_BLINK,
                       255, 255, 255,
                       times);
}

void hw_button_led_blink_center(uint8_t times) {
    if (times == 0) times = 1;
    hw_button_leds_set(BUTTON_LED_CENTER,
                       LED_MODE_BLINK,
                       255, 255, 255,
                       times);
}

void hw_button_led_blink_right(uint8_t times) {
    if (times == 0) times = 1;
    hw_button_leds_set(BUTTON_LED_RIGHT,
                       LED_MODE_BLINK,
                       255, 255, 255,
                       times);
}

// -----------------------------------------------------------------------------
// Game-state API used by protocol.c and main.c
// -----------------------------------------------------------------------------

void button_leds_set_game_state(LedGameState state) {
    g_game_state = state;
    g_ball_ready = false;

    switch (state) {
    case LED_GAME_STATE_MENU:
        apply_menu_baseline();
        break;
    case LED_GAME_STATE_IN_GAME:
        apply_ingame_baseline();
        break;
    case LED_GAME_STATE_GAME_OVER:
        apply_gameover_baseline();
        break;
    default:
        break;
    }
}

void button_leds_on_game_start(void) {
    // 5x strobe on center when game starts
    hw_button_leds_set(BUTTON_LED_CENTER,
                       LED_MODE_STROBE,
                       LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                       5);
}

void button_leds_on_ball_ready(void) {
    g_ball_ready = true;
    // Rapid blink center while ball is ready to launch
    hw_button_leds_set(BUTTON_LED_CENTER,
                       LED_MODE_RAPID_BLINK,
                       LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                       0);
}

void button_leds_on_ball_launched(void) {
    g_ball_ready = false;
    // 5x strobe on center when ball is launched
    hw_button_leds_set(BUTTON_LED_CENTER,
                       LED_MODE_STROBE,
                       LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                       5);
}

void button_leds_on_button_pressed(uint8_t button_state, uint8_t pressed_bits) {
    (void)button_state; // currently unused

    // Center button press behavior depends on game state / ball ready
    if (pressed_bits & (1u << BUTTON_CENTER_BIT)) {
        // No direct LED change here; protocol events handle animation
    }

    // Left/right: 2x strobe in menu, 1x strobe in gameplay
    if (pressed_bits & (1u << BUTTON_LEFT_BIT)) {
        if (g_game_state == LED_GAME_STATE_MENU) {
            hw_button_leds_set(BUTTON_LED_LEFT,
                               LED_MODE_STROBE,
                               LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                               2);
        } else {
            hw_button_leds_set(BUTTON_LED_LEFT,
                               LED_MODE_STROBE,
                               LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                               1);
        }
    }

    if (pressed_bits & (1u << BUTTON_RIGHT_BIT)) {
        if (g_game_state == LED_GAME_STATE_MENU) {
            hw_button_leds_set(BUTTON_LED_RIGHT,
                               LED_MODE_STROBE,
                               LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                               2);
        } else {
            hw_button_leds_set(BUTTON_LED_RIGHT,
                               LED_MODE_STROBE,
                               LED_WHITE_R, LED_WHITE_G, LED_WHITE_B,
                               1);
        }
    }
}