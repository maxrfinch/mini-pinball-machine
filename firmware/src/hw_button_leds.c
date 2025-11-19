#include "hw_button_leds.h"
#include "hw_buttons.h"
#include "pico/stdlib.h"

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
#define BLINK_INTERVAL_MS   150u
#define STROBE_INTERVAL_MS   60u
#define RAPID_BLINK_INTERVAL_MS  100u
#define BREATHE_FAST_STEP_MS     15u  // Fast breathe cycle for menu

// -----------------------------------------------------------------------------
// Hardware hook (currently stub)
// -----------------------------------------------------------------------------

// TODO: Implement this to actually drive your LED hardware (NeoKey / arcade LED board).
// For example, write to the I2C device at 0x30 and set per-button color.
static void apply_led(uint8_t idx, bool on, uint8_t r, uint8_t g, uint8_t b) {
    (void)idx;
    (void)on;
    (void)r;
    (void)g;
    (void)b;
    // Implement real hardware writes here.
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
    hw_button_leds_set(idx, g_baseline[idx].mode, g_baseline[idx].r, g_baseline[idx].g, g_baseline[idx].b, 0);
}

static void button_leds_apply_all_baselines(void) {
    for (uint8_t i = 0; i <= BUTTON_LED_RIGHT; i++) {
        button_leds_apply_baseline(i);
    }
}

static void apply_menu_baseline(void) {
    // Left: steady white
    button_leds_set_baseline(BUTTON_LED_LEFT, LED_MODE_STEADY, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Center: breathe white
    button_leds_set_baseline(BUTTON_LED_CENTER, LED_MODE_BREATHE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Right: steady white
    button_leds_set_baseline(BUTTON_LED_RIGHT, LED_MODE_STEADY, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    
    button_leds_apply_all_baselines();
}

static void apply_ingame_baseline(void) {
    // Left and right steady white in game
    button_leds_set_baseline(BUTTON_LED_LEFT, LED_MODE_STEADY, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    // Center LED off in normal gameplay (turns on for ball ready)
    button_leds_set_baseline(BUTTON_LED_CENTER, LED_MODE_OFF, 0, 0, 0);
    button_leds_set_baseline(BUTTON_LED_RIGHT, LED_MODE_STEADY, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B);
    
    button_leds_apply_all_baselines();
}

static void apply_gameover_baseline(void) {
    // Same as menu for now
    apply_menu_baseline();
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

void hw_button_leds_init(void) {
    for (int i = 0; i < 3; ++i) {
        g_leds[i].mode        = LED_MODE_OFF;
        g_leds[i].r           = 0;
        g_leds[i].g           = 0;
        g_leds[i].b           = 0;
        g_leds[i].remaining   = 0;
        g_leds[i].is_on       = false;
        g_leds[i].last_toggle = get_absolute_time();
        g_leds[i].breathe_phase = 0;
        g_leds[i].breathe_rising = true;

        apply_led((uint8_t)i, false, 0, 0, 0);
    }
    
    // Initialize to menu state with default baselines
    g_ball_ready = false;
    button_leds_set_game_state(LED_GAME_STATE_MENU);
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
        s->remaining = (count == 0) ? 1 : count; // at least one cycle
    } else {
        s->remaining = 0;
    }

    // Initialize breathe animation
    if (mode == LED_MODE_BREATHE) {
        s->breathe_phase = 0;
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
        // Initial state for breathe - start at minimum
        s->is_on = true;
        apply_led(idx, true, 0, 0, 0);
        break;

    case LED_MODE_BLINK:
    case LED_MODE_STROBE:
    case LED_MODE_RAPID_BLINK:
        // Will be handled by hw_button_leds_update()
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
            // Ensure off
            if (s->is_on) {
                s->is_on = false;
                apply_led(idx, false, 0, 0, 0);
            }
            break;

        case LED_MODE_STEADY:
            // Ensure on with set color
            if (!s->is_on) {
                s->is_on = true;
                apply_led(idx, true, s->r, s->g, s->b);
            }
            break;

        case LED_MODE_BREATHE: {
            // Smooth breathing animation
            int64_t elapsed_ms = absolute_time_diff_us(s->last_toggle, now) / 1000;
            
            if (elapsed_ms >= BREATHE_FAST_STEP_MS) {
                s->last_toggle = now;
                
                // Update breathe phase
                if (s->breathe_rising) {
                    s->breathe_phase += 8;  // Step size for smooth animation
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
                
                // Apply brightness (simple linear scaling)
                uint8_t r = (s->r * s->breathe_phase) / 255;
                uint8_t g = (s->g * s->breathe_phase) / 255;
                uint8_t b = (s->b * s->breathe_phase) / 255;
                
                apply_led(idx, true, r, g, b);
            }
            break;
        }

        case LED_MODE_BLINK:
        case LED_MODE_STROBE: {
            if (s->remaining == 0) {
                // Animation complete -> return to baseline
                s->mode = LED_MODE_OFF;
                button_leds_apply_baseline(idx);
                break;
            }

            uint32_t interval_ms =
                (s->mode == LED_MODE_BLINK) ? BLINK_INTERVAL_MS : STROBE_INTERVAL_MS;

            int64_t elapsed_ms =
                absolute_time_diff_us(s->last_toggle, now) / 1000;

            if (elapsed_ms >= (int64_t)interval_ms) {
                s->last_toggle = now;
                s->is_on = !s->is_on;

                if (s->is_on) {
                    apply_led(idx, true, s->r, s->g, s->b);
                } else {
                    apply_led(idx, false, 0, 0, 0);
                    // Completed an on+off cycle when we go back to off
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
            // Continuous rapid blink (for ball ready state)
            int64_t elapsed_ms =
                absolute_time_diff_us(s->last_toggle, now) / 1000;

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
// Convenience blink helpers (used from main.c)
// -----------------------------------------------------------------------------

void hw_button_led_blink_left(uint8_t times) {
    if (times == 0) {
        times = 1;
    }
    hw_button_leds_set(
        BUTTON_LED_LEFT,
        LED_MODE_BLINK,
        255, 255, 255, // white
        times
    );
}

void hw_button_led_blink_center(uint8_t times) {
    if (times == 0) {
        times = 1;
    }
    hw_button_leds_set(
        BUTTON_LED_CENTER,
        LED_MODE_BLINK,
        255, 255, 255, // white
        times
    );
}

void hw_button_led_blink_right(uint8_t times) {
    if (times == 0) {
        times = 1;
    }
    hw_button_leds_set(
        BUTTON_LED_RIGHT,
        LED_MODE_BLINK,
        255, 255, 255, // white
        times
    );
}

// -----------------------------------------------------------------------------
// Game-state API
// -----------------------------------------------------------------------------

void button_leds_set_game_state(LedGameState state) {
    g_game_state = state;
    g_ball_ready = false;  // Reset ball ready when changing states
    
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
    // 5x strobe on center button when game starts
    hw_button_leds_set(BUTTON_LED_CENTER, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 5);
}

void button_leds_on_ball_ready(void) {
    // Set ball ready state and start rapid blink on center button
    g_ball_ready = true;
    hw_button_leds_set(BUTTON_LED_CENTER, LED_MODE_RAPID_BLINK, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 0);
}

void button_leds_on_ball_launched(void) {
    // Clear ball ready state
    g_ball_ready = false;
    // 5x strobe on center button when ball is launched
    hw_button_leds_set(BUTTON_LED_CENTER, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 5);
}

void button_leds_on_button_pressed(uint8_t button_state, uint8_t pressed_bits) {
    (void)button_state;  // May be used in future enhancements
    
    // Handle center button press
    if (pressed_bits & (1 << BUTTON_CENTER_BIT)) {
        if (g_game_state == LED_GAME_STATE_MENU) {
            // In menu, pressing center triggers game start (handled by protocol)
            // No LED action here - game start event will trigger 5x strobe
        } else if (g_ball_ready) {
            // In game with ball ready, pressing center launches ball (handled by protocol)
            // No LED action here - ball launched event will trigger 5x strobe
        }
        // Otherwise no special center button LED action in gameplay
    }
    
    // Handle left/right button presses
    if (pressed_bits & (1 << BUTTON_LEFT_BIT)) {
        if (g_game_state == LED_GAME_STATE_MENU) {
            // 2x strobe in menu
            hw_button_leds_set(BUTTON_LED_LEFT, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 2);
        } else {
            // 1x strobe in gameplay
            hw_button_leds_set(BUTTON_LED_LEFT, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 1);
        }
    }
    
    if (pressed_bits & (1 << BUTTON_RIGHT_BIT)) {
        if (g_game_state == LED_GAME_STATE_MENU) {
            // 2x strobe in menu
            hw_button_leds_set(BUTTON_LED_RIGHT, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 2);
        } else {
            // 1x strobe in gameplay
            hw_button_leds_set(BUTTON_LED_RIGHT, LED_MODE_STROBE, LED_WHITE_R, LED_WHITE_G, LED_WHITE_B, 1);
        }
    }
}