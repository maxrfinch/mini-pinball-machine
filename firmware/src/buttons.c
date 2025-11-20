/**
 * buttons.c
 * 
 * Adafruit LED Arcade Button 1×4 I2C Breakout (Arcade Seesaw) driver
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "buttons.h"
#include "protocol.h"
#include "haptics.h"
#include "hardware_config.h"

// Seesaw registers
#define SEESAW_STATUS_BASE 0x00
#define SEESAW_GPIO_BASE 0x01
#define SEESAW_TIMER_BASE 0x08

#define SEESAW_GPIO_DIRCLR_BULK 0x03
#define SEESAW_GPIO_BULK 0x04
#define SEESAW_GPIO_PULLENSET 0x0B

static bool button_states[3] = {false, false, false};
static bool last_button_states[3] = {false, false, false};
static uint32_t button_hold_time[3] = {0, 0, 0};
static const uint32_t HOLD_THRESHOLD_MS = 500;

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
    
    // Configure buttons as inputs with pull-ups
    uint32_t mask = (1 << 0) | (1 << 1) | (1 << 2); // Buttons 0, 1, 2
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
            printf("SEESAW: read FAILED on i2c0 (error #%lu)\n", (unsigned long)error_count);
        }
        return;
    }
    
    // Log first successful read
    if (!first_success) {
        printf("SEESAW: First successful read on i2c0\n");
        first_success = true;
    }
    
    uint32_t gpio_state = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    
    // Log raw GPIO state periodically (every 500 polls ~ 5 seconds at 10ms polling)
    poll_count++;
    if ((poll_count % 500) == 0) {
        printf("SEESAW: raw GPIO = 0x%08lX (poll #%lu)\n", 
               (unsigned long)gpio_state, (unsigned long)poll_count);
    }
    
    // Buttons are active low
    button_states[BUTTON_LEFT] = !(gpio_state & (1 << 0));
    button_states[BUTTON_CENTER] = !(gpio_state & (1 << 1));
    button_states[BUTTON_RIGHT] = !(gpio_state & (1 << 2));
    
    // Check for state changes and generate events
    for (int i = 0; i < 3; i++) {
        if (button_states[i] && !last_button_states[i]) {
            // Button pressed
            const char* button_name = (i == BUTTON_LEFT) ? "LEFT" : 
                                     (i == BUTTON_CENTER) ? "CENTER" : "RIGHT";
            printf("BUTTON %s: PRESSED\n", button_name);
            
            protocol_send_button_event(i, BUTTON_STATE_DOWN);
            button_hold_time[i] = to_ms_since_boot(get_absolute_time());
            
            // Trigger haptics
            if (i == BUTTON_LEFT) {
                haptics_trigger_left();
            } else if (i == BUTTON_RIGHT) {
                haptics_trigger_right();
            } else if (i == BUTTON_CENTER) {
                haptics_trigger_both();
            }
            
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
    // LED control via Seesaw - stub for now
    // Would need to use PWM registers on Seesaw
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
