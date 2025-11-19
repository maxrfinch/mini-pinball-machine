#include "hw_buttons.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdio.h>

// Adafruit LED Arcade Button 1x4 STEMMA QT breakout (seesaw).
// Default I2C address: 0x3A (unless address jumpers have been cut).  [oai_citation:2‡Adafruit Learn](https://cdn-learn.adafruit.com/downloads/pdf/adafruit-led-arcade-button-qt.pdf?utm_source=chatgpt.com)
#define ARCADEQT_I2C_ADDR       0x3A

// RP2040 I2C config: use i2c0 on GP4 (SDA) / GP5 (SCL)
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

// On the Arcade 1x4 QT, the four switches are on seesaw pins 18, 19, 20, 2.  [oai_citation:3‡Arduino Forum](https://forum.arduino.cc/t/arduino-library-code-with-daisy-chained-adafruit-led-arcade-button-1x4-stemma-qt/1139979?utm_source=chatgpt.com)
// We only care about 3 for now (center, right, left).
#define SW_CENTER_PIN           18
#define SW_RIGHT_PIN            19
#define SW_LEFT_PIN             20
// (The 4th switch is on pin 2 if you add it later.)

// Build a bulk mask including all four switch pins
#define ARCADEQT_SWITCH_MASK    ((1u << SW_CENTER_PIN) | \
                                 (1u << SW_RIGHT_PIN)  | \
                                 (1u << SW_LEFT_PIN)   | \
                                 (1u << 2))

static uint8_t button_state = 0;

// ---- low-level helpers ---------------------------------------------------

static int arcadeqt_write(const uint8_t *buf, size_t len, bool nostop) {
    return i2c_write_blocking(I2C_PORT, ARCADEQT_I2C_ADDR, buf, len, nostop);
}

static int arcadeqt_read(uint8_t reg_base, uint8_t reg, uint8_t *dest, size_t len) {
    uint8_t cmd[2] = { reg_base, reg };

    int ret = i2c_write_blocking(I2C_PORT, ARCADEQT_I2C_ADDR, cmd, 2, false);
    if (ret < 0) {
        return ret;
    }

    // Allow some time between write and read
    sleep_us(300);

    return i2c_read_blocking(I2C_PORT, ARCADEQT_I2C_ADDR, dest, len, false);
}

// ---- public API ----------------------------------------------------------

void hw_buttons_init(void) {
    // Set up I2C on GP4/GP5
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    // Give the seesaw chip a moment to boot
    sleep_ms(10);

    // Configure the switch pins as inputs with pullups.
    uint32_t pins = ARCADEQT_SWITCH_MASK;
    uint8_t cmd[6];

    // DIRCLR_BULK: set these pins as inputs
    cmd[0] = SEESAW_GPIO_BASE;
    cmd[1] = SEESAW_GPIO_DIRCLR_BULK;
    cmd[2] = (pins >> 24) & 0xFF;
    cmd[3] = (pins >> 16) & 0xFF;
    cmd[4] = (pins >> 8)  & 0xFF;
    cmd[5] = (pins >> 0)  & 0xFF;
    arcadeqt_write(cmd, 6, false);

    // PULLENSET: enable pullups on these pins
    cmd[1] = SEESAW_GPIO_PULLENSET;
    arcadeqt_write(cmd, 6, false);

    // BULK_SET: drive them high so they behave as pullups
    cmd[1] = SEESAW_GPIO_BULK_SET;
    arcadeqt_write(cmd, 6, false);

    // INTENSET: enable interrupts (not required for polling, but harmless)
    cmd[1] = SEESAW_GPIO_INTENSET;
    arcadeqt_write(cmd, 6, false);

    // Optional debug probe so you see something at boot
    uint8_t gpio_bytes[4] = {0};
    int ret = arcadeqt_read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, gpio_bytes, 4);
    if (ret < 0) {
        printf("ArcadeQT: init I2C read failed (ret=%d). Check wiring/address.\r\n", ret);
    } else {
        uint32_t raw = (gpio_bytes[0] << 24) |
                       (gpio_bytes[1] << 16) |
                       (gpio_bytes[2] << 8)  |
                       (gpio_bytes[3] << 0);
        printf("ArcadeQT: GPIO_BULK=0x%08lx\r\n", (unsigned long)raw);
    }

    button_state = 0;
}

uint8_t hw_buttons_poll(void) {
    uint8_t gpio_bytes[4];

    int ret = arcadeqt_read(SEESAW_GPIO_BASE, SEESAW_GPIO_BULK, gpio_bytes, 4);
    if (ret < 0) {
        // Keep last known state on error
        // printf("ArcadeQT: poll I2C read failed (ret=%d)\r\n", ret);
        return button_state;
    }

    uint32_t gpio_raw = (gpio_bytes[0] << 24) |
                        (gpio_bytes[1] << 16) |
                        (gpio_bytes[2] << 8)  |
                        (gpio_bytes[3] << 0);

    // On seesaw, digital inputs are active-low:
    //   unpressed -> pin reads 1
    //   pressed   -> pin reads 0
    //
    // Map to the 3-bit protocol the Pi game expects:
    //   left   -> bit 0 (0x01)
    //   center -> bit 1 (0x02)
    //   right  -> bit 2 (0x04)
    uint8_t new_state = 0;

    // LEFT button → bit 0 (0x01)
    if (!(gpio_raw & (1u << SW_LEFT_PIN))) {
        new_state |= 0x01;
    }

    // CENTER button → bit 1 (0x02)
    if (!(gpio_raw & (1u << SW_CENTER_PIN))) {
        new_state |= 0x02;
    }

    // RIGHT button → bit 2 (0x04)
    if (!(gpio_raw & (1u << SW_RIGHT_PIN))) {
        new_state |= 0x04;
    }

    button_state = new_state;
    return button_state;
    }

uint8_t hw_buttons_get_state(void) {
    return button_state;
}