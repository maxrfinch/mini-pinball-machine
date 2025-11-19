#include "hw_buttons.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>

// Adafruit NeoKey 1x4 QT I2C (seesaw firmware)
// Default I2C address: 0x30 unless you’ve changed the A0–A3 jumpers.
#define NEOKEY_I2C_ADDR         0x30

// RP2040 I2C config: we use i2c0 on GP4/GP5
#define I2C_PORT                i2c0
#define I2C_SDA_PIN             4
#define I2C_SCL_PIN             5
#define I2C_BAUDRATE            100000

// Seesaw module bases and registers (from Adafruit seesaw docs)
#define SEESAW_STATUS_BASE      0x00
#define SEESAW_GPIO_BASE        0x01

// GPIO function registers
#define SEESAW_GPIO_DIRCLR_BULK 0x03
#define SEESAW_GPIO_BULK        0x04
#define SEESAW_GPIO_BULK_SET    0x05
#define SEESAW_GPIO_INTENSET    0x08
#define SEESAW_GPIO_PULLENSET   0x0B

// NeoKey’s four switches are on seesaw GPIO pins 4, 5, 6, 7
#define NEOKEY_BUTTON_MASK      ((1u << 4) | (1u << 5) | (1u << 6) | (1u << 7))

static uint8_t button_state = 0;
static uint8_t last_button_state = 0;

// ---- low-level helpers ---------------------------------------------------

static int neokey_write(const uint8_t *buf, size_t len, bool nostop) {
    return i2c_write_blocking(I2C_PORT, NEOKEY_I2C_ADDR, buf, len, nostop);
}

static int neokey_read(uint8_t reg_base, uint8_t reg, uint8_t *dest, size_t len) {
    uint8_t cmd[2] = { reg_base, reg };

    int ret = i2c_write_blocking(I2C_PORT, NEOKEY_I2C_ADDR, cmd, 2, false);
    if (ret < 0) {
        return ret;
    }

    // Datasheet says allow at least ~250us between write and read
    sleep_us(300);

    return i2c_read_blocking(I2C_PORT, NEOKEY_I2C_ADDR, dest, len, false);
}

// ---- public API ----------------------------------------------------------

void hw_buttons_init(void) {
    // Set up I2C on GP4/GP5
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Give the NeoKey a moment to boot
    sleep_ms(10);

    // Configure the key pins as inputs with pullups.
    uint32_t pins = NEOKEY_BUTTON_MASK;
    uint8_t cmd[6];

    // DIRCLR_BULK: set these pins as inputs
    cmd[0] = SEESAW_GPIO_BASE;
    cmd[1] = SEESAW_GPIO_DIRCLR_BULK;
    cmd[2] = (pins >> 24) & 0xFF;
    cmd[3] = (pins >> 16) & 0xFF;
    cmd[4] = (pins >> 8)  & 0xFF;
    cmd[5] = (pins >> 0)  & 0xFF;
    neokey_write(cmd, 6, false);

    // PULLENSET: enable pullups on these pins
    cmd[1] = SEESAW_GPIO_PULLENSET;
    neokey_write(cmd, 6, false);

    // BULK_SET: drive them high so they behave as pullups
    cmd[1] = SEESAW_GPIO_BULK_SET;
    neokey_write(cmd, 6, false);

    // INTENSET: enable interrupts (not required for polling, but harmless)
    cmd[1] = SEESAW_GPIO_INTENSET;
    neokey_write(cmd, 6, false);

    // Optional debug probe so you can see *something* at boot
    uint8_t gpio_bytes[4] = {0};
    int ret = neokey_read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, gpio_bytes, 4);
    if (ret < 0) {
        printf("NeoKey init: I2C read failed (ret=%d). Check wiring/address.\n", ret);
    } else {
        uint32_t raw = (gpio_bytes[0] << 24) |
                       (gpio_bytes[1] << 16) |
                       (gpio_bytes[2] << 8)  |
                       (gpio_bytes[3] << 0);
        printf("NeoKey init: GPIO_BULK=0x%08lx\n", (unsigned long)raw);
    }

    button_state = 0;
    last_button_state = 0;
}

uint8_t hw_buttons_poll(void) {
    uint8_t gpio_bytes[4];

    int ret = neokey_read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, gpio_bytes, 4);
    if (ret < 0) {
        // Keep last known state on error
        // printf("NeoKey poll: I2C read failed (ret=%d)\n", ret);
        return button_state;
    }

    uint32_t gpio_raw = (gpio_bytes[0] << 24) |
                        (gpio_bytes[1] << 16) |
                        (gpio_bytes[2] << 8)  |
                        (gpio_bytes[3] << 0);

    // On NeoKey, the buttons are active-low:
    //   unpressed -> pin reads 1
    //   pressed   -> pin reads 0
    //
    // Map:
    //   center -> seesaw pin 4 -> protocol bit 0
    //   right  -> seesaw pin 5 -> protocol bit 1
    //   left   -> seesaw pin 6 -> protocol bit 2
    //
    // (You can swap these later if physical layout doesn’t match.)
    uint8_t new_state = 0;

    if (!(gpio_raw & (1u << 4))) {
        new_state |= (1u << BUTTON_CENTER_BIT);
    }
    if (!(gpio_raw & (1u << 5))) {
        new_state |= (1u << BUTTON_RIGHT_BIT);
    }
    if (!(gpio_raw & (1u << 6))) {
        new_state |= (1u << BUTTON_LEFT_BIT);
    }

    button_state = new_state;
    last_button_state = button_state;
    return button_state;
}

uint8_t hw_buttons_get_state(void) {
    return button_state;
}