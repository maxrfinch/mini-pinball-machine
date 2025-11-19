#include "hw_button_leds.h"
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
} ButtonLEDState;

static ButtonLEDState g_leds[3];

// Timing (ms) for different modes
#define BLINK_INTERVAL_MS   150u
#define STROBE_INTERVAL_MS   60u

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

        apply_led((uint8_t)i, false, 0, 0, 0);
    }
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
        // Simple starting point: treat as steady until a more advanced
        // breathing effect is implemented.
        s->is_on = true;
        apply_led(idx, true, s->r, s->g, s->b);
        break;

    case LED_MODE_BLINK:
    case LED_MODE_STROBE:
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

        case LED_MODE_BREATHE:
            // Placeholder: current implementation just behaves like STEADY.
            if (!s->is_on) {
                s->is_on = true;
                apply_led(idx, true, s->r, s->g, s->b);
            }
            break;

        case LED_MODE_BLINK:
        case LED_MODE_STROBE: {
            if (s->remaining == 0) {
                // Animation complete -> turn off
                if (s->is_on) {
                    s->is_on = false;
                    apply_led(idx, false, 0, 0, 0);
                }
                s->mode = LED_MODE_OFF;
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
                    }
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