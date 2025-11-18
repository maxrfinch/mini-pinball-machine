#ifndef HW_SERIAL_H
#define HW_SERIAL_H

#include <stdint.h>
#include <stdbool.h>

// Initialize serial communication (USB CDC)
void hw_serial_init(void);

// Check if data is available to read
bool hw_serial_available(void);

// Read a single character (non-blocking, returns -1 if none available)
int hw_serial_getchar(void);

// Read a line into buffer (returns true if complete line read)
bool hw_serial_readline(char *buffer, uint16_t max_len);

// Write a single byte
void hw_serial_putchar(uint8_t c);

// Process incoming serial data
void hw_serial_process(void);

#endif // HW_SERIAL_H
