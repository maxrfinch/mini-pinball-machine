#ifndef UTIL_H
#define UTIL_H

// Get current time in milliseconds since epoch
long long millis(void);

// Read CPU temperature from thermal zone, returns temperature in Celsius (0-100)
// Returns -1 if unable to read
int readCpuTemperature(void);

// Read UPS battery percentage from I2C fuel gauge (Geekworm UPS at address 0x36)
// Returns battery percentage (0-100), or -1 if unable to read
int readBatteryPercent(void);

#endif // UTIL_H
