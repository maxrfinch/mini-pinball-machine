/**
 * haptics.c
 * 
 * DRV2605L haptic motor driver
 */

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "haptics.h"
#include "hardware_config.h"

// DRV2605L registers
#define DRV2605_REG_STATUS 0x00
#define DRV2605_REG_MODE 0x01
#define DRV2605_REG_WAVEFORM1 0x04
#define DRV2605_REG_WAVEFORM2 0x05
#define DRV2605_REG_GO 0x0C
#define DRV2605_REG_LIBRARY 0x03
#define DRV2605_REG_CONTROL3 0x1D

// Waveform effects
#define WAVEFORM_SHARP_CLICK 1
#define WAVEFORM_SOFT_BUZZ 10
#define WAVEFORM_LIGHT_CLICK 12

static bool haptic_write_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    int ret = i2c_write_blocking(i2c1, addr, buf, 2, false);
    return ret == 2;
}

static bool haptic_init_device(uint8_t addr) {
    sleep_ms(10);
    
    // Exit standby mode
    if (!haptic_write_reg(addr, DRV2605_REG_MODE, 0x00)) {
        return false;
    }
    
    // Set to internal trigger mode
    if (!haptic_write_reg(addr, DRV2605_REG_MODE, 0x00)) {
        return false;
    }
    
    // Select library 1 (ERM)
    if (!haptic_write_reg(addr, DRV2605_REG_LIBRARY, 0x01)) {
        return false;
    }
    
    // Configure control
    if (!haptic_write_reg(addr, DRV2605_REG_CONTROL3, 0xA0)) {
        return false;
    }
    
    return true;
}

static void haptic_play(uint8_t addr, uint8_t waveform) {
    // Set waveform
    haptic_write_reg(addr, DRV2605_REG_WAVEFORM1, waveform);
    haptic_write_reg(addr, DRV2605_REG_WAVEFORM2, 0); // End sequence
    
    // Trigger playback
    haptic_write_reg(addr, DRV2605_REG_GO, 0x01);
}

void haptics_init(void) {
    // Initialize I2C1
    i2c_init(i2c1, I2C1_FREQ);
    gpio_set_function(I2C1_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C1_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C1_SDA_PIN);
    gpio_pull_up(I2C1_SCL_PIN);
    
    sleep_ms(100);
    
    // Initialize both haptic modules
    haptic_init_device(HAPTIC_LEFT_ADDR);
    haptic_init_device(HAPTIC_RIGHT_ADDR);
}

void haptics_trigger_left(void) {
    haptic_play(HAPTIC_LEFT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_trigger_right(void) {
    haptic_play(HAPTIC_RIGHT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_trigger_both(void) {
    haptic_play(HAPTIC_LEFT_ADDR, WAVEFORM_SHARP_CLICK);
    haptic_play(HAPTIC_RIGHT_ADDR, WAVEFORM_SHARP_CLICK);
}

void haptics_light_buzz(void) {
    haptic_play(HAPTIC_LEFT_ADDR, WAVEFORM_LIGHT_CLICK);
    haptic_play(HAPTIC_RIGHT_ADDR, WAVEFORM_LIGHT_CLICK);
}
