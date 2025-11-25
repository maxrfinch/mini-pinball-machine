#include "util.h"
#include <sys/time.h>
#include <stddef.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/i2c-dev.h>
#endif

long long millis(void) {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

int readCpuTemperature(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (fp == NULL) {
        return -1;
    }
    
    int millidegrees = 0;
    if (fscanf(fp, "%d", &millidegrees) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    
    // Convert millidegrees to degrees Celsius (rounded)
    int celsius = (millidegrees + 500) / 1000;
    return celsius;
}

int readBatteryPercent(void) {
#ifdef __linux__
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) {
        return -1;
    }
    
    // Set I2C slave address to Geekworm UPS fuel gauge (0x36)
    if (ioctl(fd, I2C_SLAVE, 0x36) < 0) {
        close(fd);
        return -1;
    }
    
    // Read SOC register (0x04) - state of charge
    unsigned char reg = 0x04;
    if (write(fd, &reg, 1) != 1) {
        close(fd);
        return -1;
    }
    
    unsigned char data[2];
    if (read(fd, data, 2) != 2) {
        close(fd);
        return -1;
    }
    close(fd);
    
    // The fuel gauge returns the SOC as a 16-bit value where:
    // data[0] = LSB (fractional percentage in 1/256 increments)
    // data[1] = MSB (integer percentage 0-100)
    int percent = data[1];
    
    // Clamp to valid range (0-100)
    if (percent > 100) percent = 100;
    
    return percent;
#else
    // Not on Linux - return unavailable
    return -1;
#endif
}
