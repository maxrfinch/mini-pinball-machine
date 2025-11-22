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
#include "hardware/i2c.h"
#include "protocol.h"
#include "neopixel.h"
#include "buttons.h"
#include "display.h"

#include "debug_mode.h"
#include "onboard_neopixel.h"
#include "controller_state.h"

// Pi-centric architecture: Controller has no application state
// All state is managed by the Pi and communicated via commands

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

static void i2c0_scan(void) {
    printf("Scanning I2C0...\n");

    for (uint8_t addr = 0x08; addr < 0x78; addr++) {
        uint8_t dummy = 0;
        // Try to write 1 byte. NACK will give a negative error code.
        int res = i2c_write_blocking(i2c0, addr, &dummy, 1, true);
        if (res > 0) {
            printf("  Found device at 0x%02X\n", addr);
        }
        sleep_ms(2);
    }

    printf("I2C0 scan complete.\n");
}

int main() {
    // Initialize stdio for USB CDC
    stdio_init_all();
    
    // Small delay to let USB enumerate
    sleep_ms(1000);
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║       Adafruit KB2040 Pinball Controller v1.1            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Starting initialization sequence...\n");
    printf("\n");
    
    // Initialize hardware
    init_status_leds();
    printf("Status LEDs initialized\n\n");
    
    neopixel_init();
    printf("NeoPixels initialized\n\n");
    
    onboard_neopixel_init();
    printf("Onboard NeoPixel initialized (GPIO 17)\n\n");
    
    buttons_init();
    display_init();
    
    protocol_init();
    printf("Protocol initialized\n\n");
    
    debug_mode_init();
    printf("Debug mode initialized\n\n");
    
    // Initialize controller
    controller_state_init();
    printf("Controller initialized (Pi-centric mode)\n\n");
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  SYSTEM READY                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Awaiting commands via USB serial...\n");
    printf("Use 'CMD DEBUG' to manually enter debug mode.\n");
    printf("\n");
    
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
        
        // Check event timeouts and revert to base priority if needed
        controller_check_event_timeouts();
        
        // Update animations
        if (debug_mode_is_active()) {
            debug_mode_update();
        } else {
            neopixel_update_effect();
            buttons_update_leds();
        }
        
        // Update onboard NeoPixel (always running)
        onboard_neopixel_update();
        
        // Update display animations
        display_update_animation();
        
        // Update displays
        display_update();
        //i2c0_scan();

        
        // Small delay to prevent tight loop
        sleep_ms(10);
    }
    
    return 0;
}
