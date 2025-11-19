#include "pico/stdlib.h"
#include "hw_serial.h"
#include "hw_buttons.h"
#include "hw_display.h"
#include "hw_button_leds.h"
#include "protocol.h"
// Phase 2 & 3 includes (stubs only for now)
// #include "hw_neopixel.h"
// #include "hw_haptics.h"
#include <string.h>
#include <stdio.h>

#define CMD_BUFFER_SIZE 128
#define BUTTON_POLL_INTERVAL_MS 10

int main(void) {
    // Initialize Pico stdio (USB CDC)
    stdio_init_all();
    
    // Small delay to let USB enumerate
    sleep_ms(1000);
    
    // Initialize hardware subsystems (Phase 1)
    hw_buttons_init();
    hw_display_init();
    hw_serial_init();
    hw_button_leds_init();
    protocol_init();

    // Boot banner for USB debug
    printf("Pico firmware booted\n");
    
    // Phase 2 & 3 initialization (TODO: uncomment when implemented)
    // hw_neopixel_init();
    // hw_haptics_init();
    
    // Clear display on startup
    hw_display_clear();
    hw_display_refresh();
    
    // Main event loop
    char cmd_buffer[CMD_BUFFER_SIZE];
    uint8_t last_button_state = 0;
    absolute_time_t last_button_poll = get_absolute_time();
    
    while (true) {
        // Poll buttons at regular intervals
        if (absolute_time_diff_us(last_button_poll, get_absolute_time()) >= (BUTTON_POLL_INTERVAL_MS * 1000)) {
            uint8_t button_state = hw_buttons_poll();
            
            // Detect changes and only react on 0 -> 1 transitions (new presses)
            if (button_state != last_button_state) {
                uint8_t changed = button_state ^ last_button_state;
                uint8_t pressed = button_state & changed; // bits that just went high

                // Send raw button state over the serial protocol
                hw_serial_putchar(button_state);

                // USB debug output
                printf("buttons=%u\n", button_state);
                if (pressed & 0x04) {
                    printf("LEFT pressed\n");
                    hw_button_led_blink_left(2);    // double blink on left press
                }
                if (pressed & 0x01) {
                    printf("CENTER pressed\n");
                    hw_button_led_blink_center(3);  // triple blink on center press
                }
                if (pressed & 0x02) {
                    printf("RIGHT pressed\n");
                    hw_button_led_blink_right(1);   // single blink on right press
                }

                last_button_state = button_state;
            }
            
            last_button_poll = get_absolute_time();
        }
        
        // Process incoming serial commands
        if (hw_serial_readline(cmd_buffer, CMD_BUFFER_SIZE)) {
            protocol_process_command(cmd_buffer);
        }
        
        // Update button LEDs
        hw_button_leds_update();
        
        // Future hardware updates would go here:
        // update_neopixels();
        // update_haptics();
        
        // Small delay to prevent tight loop
        sleep_ms(1);
    }
    
    return 0;
}
