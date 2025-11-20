/**
 * haptics.c
 * 
 * DRV2605L haptic motor driver
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "haptics.h"
#include "hardware_config.h"

// DRV2605L registers
#define DRV2605_REG_STATUS    0x00
#define DRV2605_REG_MODE      0x01
#define DRV2605_REG_LIBRARY   0x03
#define DRV2605_REG_WAVEFORM1 0x04
#define DRV2605_REG_WAVEFORM2 0x05
#define DRV2605_REG_GO        0x0C
#define DRV2605_REG_CONTROL3  0x1D

// Waveform effects (library 1 examples)
#define WAVEFORM_SHARP_CLICK  1
#define WAVEFORM_SOFT_BUZZ    10
#define WAVEFORM_LIGHT_CLICK  12

// ---------------------------------------------------------------------------
// Low-level helpers
// ---------------------------------------------------------------------------

static bool haptic_write_reg(i2c_inst_t *bus, uint8_t addr,
                             uint8_t reg, uint8_t value) {
    uint8_t buf[2] = { reg, value };
    int ret = i2c_write_blocking(bus, addr, buf, 2, false);
    return ret == 2;
}

static bool haptic_init_device(i2c_inst_t *bus, uint8_t addr, const char *label) {
    printf("Initializing DRV2605L %s at 0x%02X...\n", label, addr);
    sleep_ms(10);

    // Exit standby mode, internal trigger mode (MODE = 0x00)
    if (!haptic_write_reg(bus, addr, DRV2605_REG_MODE, 0x00)) {
        printf("  [%s] Failed to exit standby / set mode\n", label);
        return false;
    }

    // Select library 1 (ERM)
    if (!haptic_write_reg(bus, addr, DRV2605_REG_LIBRARY, 0x01)) {
        printf("  [%s] Failed to select library\n", label);
        return false;
    }

    // Configure CONTROL3
    // 0xA0 is a typical config (see Adafruit examples) –
    // it enables ERM open-loop and some default settings.
    if (!haptic_write_reg(bus, addr, DRV2605_REG_CONTROL3, 0xA0)) {
        printf("  [%s] Failed to configure CONTROL3\n", label);
        return false;
    }

    printf("  DRV2605L %s (0x%02X) initialized successfully\n", label, addr);
    return true;
}

static void haptic_play(i2c_inst_t *bus, uint8_t addr, uint8_t waveform) {
    // Set waveform sequence: [waveform, 0x00] where 0 ends the sequence
    (void)haptic_write_reg(bus, addr, DRV2605_REG_WAVEFORM1, waveform);
    (void)haptic_write_reg(bus, addr, DRV2605_REG_WAVEFORM2, 0x00);

    // Trigger playback
    (void)haptic_write_reg(bus, addr, DRV2605_REG_GO, 0x01);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void haptics_init(void) {
    printf("\n=== Haptics Initialization ===\n");

    // I2C1 is dedicated to the LEFT haptic in this project.
    printf("Initializing LEFT haptic on I2C1 at %d Hz on GPIO%d (SDA) / GPIO%d (SCL)\n",
           I2C1_FREQ, I2C1_SDA_PIN, I2C1_SCL_PIN);

    i2c_init(i2c1, I2C1_FREQ);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);

    printf("I2C1 hardware initialized for LEFT haptic\n");

    // NOTE: I2C0 (for RIGHT haptic) is initialized elsewhere for the
    // arcade Seesaw + matrix displays. We assume it is ready here.

    sleep_ms(100);

    // Initialize both haptic modules
    bool left_ok  = haptic_init_device(i2c1, HAPTIC_LEFT_ADDR,  "LEFT (I2C1)");
    bool right_ok = haptic_init_device(i2c0, HAPTIC_RIGHT_ADDR, "RIGHT (I2C0 via STEMMA)");

    if (!left_ok) {
        printf("WARNING: LEFT haptic failed to initialize\n");
    }
    if (!right_ok) {
        printf("WARNING: RIGHT haptic failed to initialize\n");
    }

    printf("=== Haptics Initialization Complete ===\n\n");
}

void haptics_trigger_left(void) {
    haptic_play(i2c1, HAPTIC_LEFT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_trigger_right(void) {
    haptic_play(i2c0, HAPTIC_RIGHT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_trigger_both(void) {
    haptic_play(i2c1, HAPTIC_LEFT_ADDR,  WAVEFORM_SHARP_CLICK);
    haptic_play(i2c0, HAPTIC_RIGHT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_light_buzz(void) {
    haptic_play(i2c1, HAPTIC_LEFT_ADDR,  WAVEFORM_LIGHT_CLICK);
    haptic_play(i2c0, HAPTIC_RIGHT_ADDR, WAVEFORM_LIGHT_CLICK);
}
