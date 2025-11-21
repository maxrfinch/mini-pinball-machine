/**
 * buttons.c
 * 
 * Adafruit LED Arcade Button 1×4 I2C Breakout (Arcade Seesaw) driver
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "buttons.h"
#include "protocol.h"
#include "hardware_config.h"
#include "controller_state.h"

// Seesaw registers
#define SEESAW_STATUS_BASE 0x00
#define SEESAW_GPIO_BASE 0x01
#define SEESAW_TIMER_BASE 0x08

#define SEESAW_GPIO_DIRCLR_BULK 0x03
#define SEESAW_GPIO_BULK 0x04
#define SEESAW_GPIO_PULLENSET 0x0B

// Seesaw timer/PWM function
#define SEESAW_TIMER_PWM 0x01

// Seesaw LED pin numbers for the Arcade QT board
// LED pins in order (1, 2, 3, 4) per Adafruit example: 12, 13, 0, 1
#define SEESAW_LED_PIN_LEFT   12
#define SEESAW_LED_PIN_CENTER 13
#define SEESAW_LED_PIN_RIGHT  0

// Seesaw GPIO pin numbers for the Arcade QT buttons
// Button pins in order (1, 2, 3, 4) per Adafruit example: 18, 19, 20, 2
#define SEESAW_BTN_PIN_0 18  // logical button index 0
#define SEESAW_BTN_PIN_1 19  // logical button index 1
#define SEESAW_BTN_PIN_2 20  // logical button index 2

static bool button_states[3] = {false, false, false};
static bool last_button_states[3] = {false, false, false};
static uint32_t button_hold_time[3] = {0, 0, 0};
static const uint32_t HOLD_THRESHOLD_MS = 500;

// Button LED effect state
static ButtonLEDEffect current_effect = BTN_EFFECT_OFF;
static uint32_t effect_start_time = 0;
static uint32_t effect_frame = 0;
static uint8_t led_brightness[3] = {0, 0, 0};
static Button menu_selection = BUTTON_LEFT;

// Effect timing constants
#define TWO_PI 6.28318530718f
#define BALL_SAVED_CYCLES 8
#define BALL_SAVED_CYCLE_INCREMENT_MS 20
#define EXTRA_BALL_PULSE_ON_MS 200
#define EXTRA_BALL_PULSE_OFF_MS 100
#define EXTRA_BALL_PULSE_PERIOD_MS (EXTRA_BALL_PULSE_ON_MS + EXTRA_BALL_PULSE_OFF_MS)

// Pseudo-random constants for chaotic effects
#define PRNG_MULT_1 137
#define PRNG_ADD_1 53
#define PRNG_OFFSET_1 17
#define PRNG_MULT_2 149
#define PRNG_ADD_2 71
#define PRNG_OFFSET_2 31
#define PRNG_MULT_3 163
#define PRNG_ADD_3 89

static uint8_t button_to_led_pin(Button button) {
    switch (button) {
        case BUTTON_LEFT:   return SEESAW_LED_PIN_LEFT;
        case BUTTON_CENTER: return SEESAW_LED_PIN_CENTER;
        case BUTTON_RIGHT:  return SEESAW_LED_PIN_RIGHT;
        default:            return 0xFF; // invalid
    }
}

static bool seesaw_write(uint8_t reg_high, uint8_t reg_low, const uint8_t* data, size_t len) {
    uint8_t buf[32];
    buf[0] = reg_high;
    buf[1] = reg_low;
    if (len > 0 && data != NULL) {
        memcpy(buf + 2, data, len);
    }
    int ret = i2c_write_blocking(i2c0, SEESAW_ADDR, buf, 2 + len, false);
    return ret == (2 + len);
}

static bool seesaw_read(uint8_t reg_high, uint8_t reg_low, uint8_t* data, size_t len) {
    uint8_t buf[2] = {reg_high, reg_low};
    int ret = i2c_write_blocking(i2c0, SEESAW_ADDR, buf, 2, true);
    if (ret != 2) return false;
    
    sleep_ms(1);
    ret = i2c_read_blocking(i2c0, SEESAW_ADDR, data, len, false);
    return ret == len;
}

void buttons_init(void) {
    printf("\n=== Button Initialization (I2C0 - Hardware I2C) ===\n");
    printf("Initializing I2C0 at %d Hz on GPIO%d (SDA) / GPIO%d (SCL)\n",
           I2C0_FREQ, I2C0_SDA_PIN, I2C0_SCL_PIN);
    
    // Initialize I2C0 for arcade seesaw buttons
    i2c_init(i2c0, I2C0_FREQ);
    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C0_SDA_PIN);
    gpio_pull_up(I2C0_SCL_PIN);
    
    printf("I2C0 hardware initialized\n");
    
    sleep_ms(100);
    
    printf("Configuring Seesaw at address 0x%02X...\n", SEESAW_ADDR);
    
    // Configure buttons as inputs with pull-ups (use Seesaw GPIO pins 18, 19, 20)
    uint32_t mask = (1u << SEESAW_BTN_PIN_0) |
                    (1u << SEESAW_BTN_PIN_1) |
                    (1u << SEESAW_BTN_PIN_2);
    uint8_t mask_bytes[4] = {
        (mask >> 24) & 0xFF,
        (mask >> 16) & 0xFF,
        (mask >> 8) & 0xFF,
        mask & 0xFF
    };
    
    // Set as inputs
    if (!seesaw_write(SEESAW_GPIO_BASE, SEESAW_GPIO_DIRCLR_BULK, mask_bytes, 4)) {
        printf("  WARNING: Failed to set button direction (Seesaw not responding?)\n");
    } else {
        printf("  Button direction configured\n");
    }
    sleep_ms(10);
    
    // Enable pull-ups
    if (!seesaw_write(SEESAW_GPIO_BASE, SEESAW_GPIO_PULLENSET, mask_bytes, 4)) {
        printf("  WARNING: Failed to enable button pull-ups\n");
    } else {
        printf("  Button pull-ups enabled\n");
    }
    sleep_ms(10);
    
    printf("=== Button Initialization Complete ===\n\n");
}

void buttons_poll(void) {
    static uint32_t poll_count = 0;
    static uint32_t error_count = 0;
    static bool first_success = false;
    
    uint8_t data[4];
    if (!seesaw_read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, data, 4)) {
        error_count++;
        // Log first few errors and then periodically
        if (error_count <= 5 || (error_count % 100) == 0) {
            //printf("SEESAW: read FAILED on i2c0 (error #%lu)\n", (unsigned long)error_count);
        }
        return;
    }
    
    // Log first successful read
    if (!first_success) {
        printf("SEESAW: First successful read on i2c0\n");
        first_success = true;
    }
    
    uint32_t gpio_state = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    // Buttons are active low; map Seesaw GPIO pins to logical buttons
    button_states[BUTTON_LEFT]   = !(gpio_state & (1u << SEESAW_BTN_PIN_0));
    button_states[BUTTON_CENTER] = !(gpio_state & (1u << SEESAW_BTN_PIN_1));
    button_states[BUTTON_RIGHT]  = !(gpio_state & (1u << SEESAW_BTN_PIN_2));
    
    // Check for state changes and generate events
    for (int i = 0; i < 3; i++) {
        if (button_states[i] && !last_button_states[i]) {
            // Button pressed
            const char* button_name = (i == BUTTON_LEFT) ? "LEFT" : 
                                     (i == BUTTON_CENTER) ? "CENTER" : "RIGHT";
            printf("BUTTON %s: PRESSED\n", button_name);
            
            // Let controller state machine handle button press first
            bool handled = controller_handle_button_press(i);
            
            // Always send raw button event to host (requirement #8)
            protocol_send_button_event(i, BUTTON_STATE_DOWN);
            button_hold_time[i] = to_ms_since_boot(get_absolute_time());
            
        } else if (!button_states[i] && last_button_states[i]) {
            // Button released
            const char* button_name = (i == BUTTON_LEFT) ? "LEFT" : 
                                     (i == BUTTON_CENTER) ? "CENTER" : "RIGHT";
            printf("BUTTON %s: RELEASED\n", button_name);
            
            protocol_send_button_event(i, BUTTON_STATE_UP);
            button_hold_time[i] = 0;
            
        } else if (button_states[i] && last_button_states[i]) {
            // Button held
            uint32_t now = to_ms_since_boot(get_absolute_time());
            if (button_hold_time[i] > 0 && (now - button_hold_time[i]) > HOLD_THRESHOLD_MS) {
                protocol_send_button_event(i, BUTTON_STATE_HELD);
                button_hold_time[i] = 0; // Prevent repeated HELD events
            }
        }
        
        last_button_states[i] = button_states[i];
    }
}

void buttons_set_led(Button button, uint8_t brightness) {
    uint8_t pin = button_to_led_pin(button);
    if (pin == 0xFF) {
        return; // invalid button
    }

    // Map 0–255 brightness to 16-bit duty (0–65535)
    uint16_t duty = (uint16_t)brightness * 257u; // 255 -> ~65535

    uint8_t payload[3];
    payload[0] = pin;                 // PWM channel / pin
    payload[1] = (duty >> 8) & 0xFF;  // high byte
    payload[2] = duty & 0xFF;         // low byte

    // Write PWM duty cycle to Seesaw timer module
    (void)seesaw_write(SEESAW_TIMER_BASE, SEESAW_TIMER_PWM, payload, 3);
}

void buttons_set_led_pulse(Button button, bool slow) {
    // LED pulse control - stub for now
}

bool buttons_is_pressed(Button button) {
    if (button < 3) {
        return button_states[button];
    }
    return false;
}

void buttons_start_effect(ButtonLEDEffect effect) {
    current_effect = effect;
    effect_start_time = to_ms_since_boot(get_absolute_time());
    effect_frame = 0;
}

void buttons_set_menu_selection(Button button) {
    menu_selection = button;
}

// Helper: smooth sine wave breathing (0-255)
static uint8_t breathe_sine(uint32_t time_ms, uint32_t period_ms, uint32_t phase_offset_ms) {
    uint32_t phase = (time_ms + phase_offset_ms) % period_ms;
    float angle = (float)phase / period_ms * TWO_PI;
    float sine_val = (sinf(angle) + 1.0f) / 2.0f; // 0.0 to 1.0
    return (uint8_t)(sine_val * 255.0f);
}

// Helper: linear ramp (0-255)
static uint8_t ramp_linear(uint32_t time_ms, uint32_t period_ms) {
    uint32_t phase = time_ms % period_ms;
    return (uint8_t)((phase * 255) / period_ms);
}

// Helper: triangle wave
static uint8_t triangle_wave(uint32_t time_ms, uint32_t period_ms) {
    uint32_t phase = time_ms % period_ms;
    if (phase < period_ms / 2) {
        return (uint8_t)((phase * 255 * 2) / period_ms);
    } else {
        return (uint8_t)(255 - ((phase - period_ms / 2) * 255 * 2) / period_ms);
    }
}

void buttons_update_leds(void) {
    uint32_t elapsed_ms = to_ms_since_boot(get_absolute_time()) - effect_start_time;
    effect_frame++;
    
    switch (current_effect) {
        case BTN_EFFECT_OFF:
            led_brightness[BUTTON_LEFT] = 0;
            led_brightness[BUTTON_CENTER] = 0;
            led_brightness[BUTTON_RIGHT] = 0;
            break;
            
        case BTN_EFFECT_READY_STEADY_GLOW: {
            // Slow breathing fade (2-3 Hz = 333-500ms period)
            // LEFT: 0 → 180 → 0 with sine wave
            // RIGHT: same but shifted by +1000ms
            // CENTER: steady 200
            uint32_t period_ms = 400; // ~2.5 Hz
            led_brightness[BUTTON_LEFT] = (breathe_sine(elapsed_ms, period_ms, 0) * 180) / 255;
            led_brightness[BUTTON_RIGHT] = (breathe_sine(elapsed_ms, period_ms, 1000) * 180) / 255;
            led_brightness[BUTTON_CENTER] = 200;
            break;
        }
            
        case BTN_EFFECT_FLIPPER_FEEDBACK: {
            // Fast pop to max, immediate drop, return to previous
            // This should be triggered on button press, not continuous
            // For now, implement as a one-shot 100ms burst
            if (elapsed_ms < 50) {
                led_brightness[BUTTON_LEFT] = 255;
                led_brightness[BUTTON_RIGHT] = 255;
            } else if (elapsed_ms < 100) {
                led_brightness[BUTTON_LEFT] = 0;
                led_brightness[BUTTON_RIGHT] = 0;
            } else {
                // Return to attract mode or previous state
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
            }
            led_brightness[BUTTON_CENTER] = 200;
            break;
        }
            
        case BTN_EFFECT_CENTER_HIT_PULSE: {
            // Rapid double-strobe: 2 bursts at 40ms each, then 300ms dark, loop
            uint32_t cycle = elapsed_ms % 380; // 40+40+300
            if (cycle < 40) {
                led_brightness[BUTTON_CENTER] = 255;
            } else if (cycle < 80) {
                led_brightness[BUTTON_CENTER] = 0;
            } else if (cycle < 120) {
                led_brightness[BUTTON_CENTER] = 255;
            } else {
                led_brightness[BUTTON_CENTER] = 0;
            }
            led_brightness[BUTTON_LEFT] = 0;
            led_brightness[BUTTON_RIGHT] = 0;
            break;
        }
            
        case BTN_EFFECT_SKILL_SHOT_BUILDUP: {
            // Ramp 0→255 over 2 seconds, snap to 0, repeat
            uint32_t cycle = elapsed_ms % 2000;
            uint8_t brightness = ramp_linear(cycle, 2000);
            led_brightness[BUTTON_LEFT] = brightness;
            led_brightness[BUTTON_CENTER] = brightness;
            led_brightness[BUTTON_RIGHT] = brightness;
            break;
        }
            
        case BTN_EFFECT_BALL_SAVED: {
            // Alternating flash back/forth, 5-8 cycles with decaying speed
            // Start fast, get slower. Let's do 8 cycles over ~2 seconds
            uint32_t total_duration = 2000;
            if (elapsed_ms > total_duration) {
                // Effect complete, return to attract
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
                break;
            }
            
            // Divide into cycles, each getting progressively longer
            // Simple approach: use accelerating period
            uint32_t cycle_base = 100; // Start at 100ms per toggle
            uint32_t time_acc = 0;
            bool left_on = false;
            
            for (int i = 0; i < BALL_SAVED_CYCLES; i++) {
                uint32_t cycle_period = cycle_base + (i * BALL_SAVED_CYCLE_INCREMENT_MS);
                time_acc += cycle_period;
                if (elapsed_ms < time_acc) {
                    left_on = (i % 2) == 0;
                    break;
                }
            }
            
            led_brightness[BUTTON_LEFT] = left_on ? 255 : 0;
            led_brightness[BUTTON_RIGHT] = left_on ? 0 : 255;
            led_brightness[BUTTON_CENTER] = 0;
            break;
        }
            
        case BTN_EFFECT_POWERUP_ALERT: {
            // Chaotic strobe with randomized brightness, 1.5s max
            if (elapsed_ms > 1500) {
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
                break;
            }
            
            // Pseudo-random using frame counter
            uint8_t rand_val = (effect_frame * PRNG_MULT_1 + PRNG_ADD_1) & 0xFF;
            led_brightness[BUTTON_LEFT] = (rand_val < 128) ? 255 : 0;
            
            rand_val = ((effect_frame + PRNG_OFFSET_1) * PRNG_MULT_2 + PRNG_ADD_2) & 0xFF;
            led_brightness[BUTTON_CENTER] = (rand_val < 128) ? 255 : 0;
            
            rand_val = ((effect_frame + PRNG_OFFSET_2) * PRNG_MULT_3 + PRNG_ADD_3) & 0xFF;
            led_brightness[BUTTON_RIGHT] = (rand_val < 128) ? 255 : 0;
            break;
        }
            
        case BTN_EFFECT_EXTRA_BALL_AWARD: {
            // Three medium-speed pulses, then one long fade-out
            // Total: 3*(PULSE_ON+PULSE_OFF) = 900ms, then 1000ms fade = 1900ms total
            if (elapsed_ms < 900) {
                uint32_t pulse_cycle = elapsed_ms % EXTRA_BALL_PULSE_PERIOD_MS;
                uint8_t brightness = (pulse_cycle < EXTRA_BALL_PULSE_ON_MS) ? 255 : 0;
                led_brightness[BUTTON_CENTER] = brightness;
            } else if (elapsed_ms < 1900) {
                // Fade out over 1000ms
                uint32_t fade_time = elapsed_ms - 900;
                led_brightness[BUTTON_CENTER] = 255 - (uint8_t)((fade_time * 255) / 1000);
            } else {
                // Effect complete
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
            }
            led_brightness[BUTTON_LEFT] = 0;
            led_brightness[BUTTON_RIGHT] = 0;
            break;
        }
            
        case BTN_EFFECT_GAME_OVER_FADE: {
            // Extremely slow fade: bright → off → bright → off (2.5s cycle)
            uint32_t brightness = triangle_wave(elapsed_ms, 2500);
            led_brightness[BUTTON_LEFT] = brightness;
            led_brightness[BUTTON_CENTER] = brightness;
            led_brightness[BUTTON_RIGHT] = brightness;
            break;
        }
            
        case BTN_EFFECT_MENU_NAVIGATION: {
            // Left/Right at 160, selected button at 255, others dim to 80
            for (int i = 0; i < 3; i++) {
                if (i == menu_selection) {
                    led_brightness[i] = 255;
                } else {
                    led_brightness[i] = 80;
                }
            }
            break;
        }
    }
    
    // Apply brightness to physical LEDs
    buttons_set_led(BUTTON_LEFT, led_brightness[BUTTON_LEFT]);
    buttons_set_led(BUTTON_CENTER, led_brightness[BUTTON_CENTER]);
    buttons_set_led(BUTTON_RIGHT, led_brightness[BUTTON_RIGHT]);
}
