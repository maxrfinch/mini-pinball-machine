#include "hw_buttons.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Adafruit NeoKey 1x4 QT I2C Stemma board
// Default I2C address: 0x30
#define NEOKEY_I2C_ADDR     0x30
#define NEOKEY_REG_STATUS   0x00
#define NEOKEY_REG_BUTTON   0x10

// I2C configuration
#define I2C_PORT i2c0
#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_BAUDRATE 100000

static uint8_t button_state = 0;
static uint8_t last_button_state = 0;

void hw_buttons_init(void) {
    // Initialize I2C
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
}

uint8_t hw_buttons_poll(void) {
    uint8_t data[4];
    
    // Read button states from NeoKey board
    // The board has 4 buttons at register 0x10-0x13
    uint8_t reg = NEOKEY_REG_BUTTON;
    int ret = i2c_write_blocking(I2C_PORT, NEOKEY_I2C_ADDR, &reg, 1, true);
    if (ret < 0) {
        // I2C error - return last known state
        return button_state;
    }
    
    ret = i2c_read_blocking(I2C_PORT, NEOKEY_I2C_ADDR, data, 4, false);
    if (ret < 0) {
        // I2C error - return last known state
        return button_state;
    }
    
    // Map QT board buttons to bit positions
    // QT Index 0 (Left) -> Bit 2
    // QT Index 1 (Center) -> Bit 0
    // QT Index 2 (Right) -> Bit 1
    // QT Index 3 -> Reserved (not used)
    
    button_state = 0;
    if (data[0]) button_state |= (1 << BUTTON_LEFT_BIT);    // Bit 2
    if (data[1]) button_state |= (1 << BUTTON_CENTER_BIT);  // Bit 0
    if (data[2]) button_state |= (1 << BUTTON_RIGHT_BIT);   // Bit 1
    
    // Send button state if it changed
    if (button_state != last_button_state) {
        last_button_state = button_state;
    }
    
    return button_state;
}

uint8_t hw_buttons_get_state(void) {
    return button_state;
}
