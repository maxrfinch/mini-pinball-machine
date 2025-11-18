#include "hw_serial.h"
#include "pico/stdlib.h"
#include <string.h>

#define SERIAL_BUFFER_SIZE 128

static char rx_buffer[SERIAL_BUFFER_SIZE];
static uint16_t rx_index = 0;

void hw_serial_init(void) {
    // USB CDC is initialized by stdio_init_all() in main
    // This is just a placeholder for any additional setup
}

bool hw_serial_available(void) {
    return getchar_timeout_us(0) != PICO_ERROR_TIMEOUT;
}

int hw_serial_getchar(void) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) {
        return -1;
    }
    return c;
}

bool hw_serial_readline(char *buffer, uint16_t max_len) {
    int c = hw_serial_getchar();
    
    if (c == -1) {
        return false;
    }
    
    if (c == '\n' || c == '\r') {
        if (rx_index > 0) {
            // Copy to output buffer
            uint16_t len = (rx_index < max_len - 1) ? rx_index : (max_len - 1);
            memcpy(buffer, rx_buffer, len);
            buffer[len] = '\0';
            rx_index = 0;
            return true;
        }
        return false;
    }
    
    // Add character to buffer if space available
    if (rx_index < SERIAL_BUFFER_SIZE - 1) {
        rx_buffer[rx_index++] = (char)c;
    }
    
    return false;
}

void hw_serial_putchar(uint8_t c) {
    putchar_raw(c);
}

void hw_serial_process(void) {
    // This function is called from main loop to process any waiting data
    // Actual processing happens in hw_serial_readline()
}
