/**
 * main.c
 * 
 * Main entry point and event loop for Pico pinball controller
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/time.h"
#include "hardware/gpio.h"
#include "hardware_config.h"
#include "protocol.h"
#include "neopixel.h"
#include "buttons.h"
#include "haptics.h"
#include "display.h"
#include "debug_mode.h"

// Status LED blink for heartbeat
static absolute_time_t last_heartbeat = 0;
static bool heartbeat_state = false;

void init_status_leds(void) {
    gpio_init(STATUS_LED_PIN);
    gpio_set_dir(STATUS_LED_PIN, GPIO_OUT);
    gpio_init(MODE_LED_PIN);
    gpio_set_dir(MODE_LED_PIN, GPIO_OUT);
}

void update_heartbeat(void) {
    absolute_time_t now = get_absolute_time();
    if (absolute_time_diff_us(last_heartbeat, now) > 1000000) { // 1 second
        heartbeat_state = !heartbeat_state;
        gpio_put(STATUS_LED_PIN, heartbeat_state);
        last_heartbeat = now;
    }
}

int main() {
    // Initialize stdio for USB CDC
    stdio_init_all();
    
    // Small delay to let USB enumerate
    sleep_ms(100);
    
    // Initialize hardware
    init_status_leds();
    neopixel_init();
    buttons_init();
    haptics_init();
    display_init();
    protocol_init();
    debug_mode_init();
    
    // Send READY event
    sleep_ms(100);
    protocol_send_ready();
    
    // Main event loop
    while (1) {
        // Update heartbeat LED
        update_heartbeat();
        
        // Process incoming commands
        protocol_process();
        
        // Poll button states
        buttons_poll();
        
        // Check for debug mode timeout
        debug_mode_check();
        
        // Update animations
        if (debug_mode_is_active()) {
            debug_mode_update();
        } else {
            neopixel_update_effect();
        }
        
        // Update displays
        display_update();
        
        // Small delay to prevent tight loop
        sleep_ms(10);
    }
    
    return 0;
}
