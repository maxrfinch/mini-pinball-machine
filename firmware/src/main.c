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
#include "haptics.h"
#include "display.h"

#include "debug_mode.h"

// Application-level mode and effect enums matching the command protocol

typedef enum {
    APP_MODE_ATTRACT = 0,
    APP_MODE_MENU,
    APP_MODE_GAME,
    APP_MODE_BALL_LOST,
    APP_MODE_HIGH_SCORE
} AppMode;

typedef enum {
    APP_EFFECT_NONE = 0,
    APP_EFFECT_RAINBOW_BREATHE,
    APP_EFFECT_RAINBOW_WAVE,
    APP_EFFECT_CAMERA_FLASH,
    APP_EFFECT_RED_STROBE_5X,
    APP_EFFECT_WATER,
    APP_EFFECT_ATTRACT,
    APP_EFFECT_PINK_PULSE,
    APP_EFFECT_BALL_LAUNCH
} AppEffect;

// Core application state driven by CMD MODE / CMD EFFECT / CMD SCORE / CMD BALLS / CMD BRIGHTNESS
static AppMode   g_app_mode        = APP_MODE_ATTRACT;
static AppEffect g_app_effect      = APP_EFFECT_ATTRACT;
static uint32_t  g_app_score       = 0;
static uint8_t   g_app_balls       = 3;
static uint8_t   g_app_brightness  = 255;

// Forward declarations for core handlers
void app_set_mode(AppMode mode);
void app_set_effect(AppEffect effect);
void app_set_score(uint32_t score);
void app_set_balls(uint8_t balls);
void app_set_brightness(uint8_t brightness);

void app_set_mode(AppMode mode) {
    g_app_mode = mode;

    // Simple mode LED behavior: ON in GAME, OFF otherwise
    switch (mode) {
        case APP_MODE_GAME:
            gpio_put(MODE_LED_PIN, 1);
            break;
        default:
            gpio_put(MODE_LED_PIN, 0);
            break;
    }

    printf("[APP] MODE set to %d\n", (int)mode);
}

void app_set_effect(AppEffect effect) {
    g_app_effect = effect;
    printf("[APP] EFFECT set to %d\n", (int)effect);
}

void app_set_score(uint32_t score) {
    g_app_score = score;
    printf("[APP] SCORE = %lu\n", (unsigned long)score);
}

void app_set_balls(uint8_t balls) {
    g_app_balls = balls;
    printf("[APP] BALLS = %u\n", (unsigned)balls);
}

void app_set_brightness(uint8_t brightness) {
    g_app_brightness = brightness;
    printf("[APP] BRIGHTNESS = %u\n", (unsigned)brightness);
}

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
    printf("║        Raspberry Pi Pico Pinball Controller v1.0         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Starting initialization sequence...\n");
    printf("\n");
    
    // Initialize hardware
    init_status_leds();
    printf("Status LEDs initialized\n\n");
    
    neopixel_init();
    printf("NeoPixels initialized\n\n");
    
    buttons_init();
    haptics_init();
    display_init();
    
    protocol_init();
    printf("Protocol initialized\n\n");
    
    debug_mode_init();
    printf("Debug mode initialized\n\n");
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║                  SYSTEM READY                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Awaiting commands via USB serial...\n");
    printf("Debug mode will activate after 30 seconds of inactivity.\n");
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
        //i2c0_scan();

        
        // Small delay to prevent tight loop
        sleep_ms(10);
    }
    
    return 0;
}
