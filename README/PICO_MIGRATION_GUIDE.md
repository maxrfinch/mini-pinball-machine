# Pico Migration Guide: Arduino Uno → Raspberry Pi Pico

This guide explains how to migrate from the original Arduino Uno firmware to the new Raspberry Pi Pico firmware.

## Why Migrate?

The Raspberry Pi Pico offers several advantages over the Arduino Uno:

1. **More GPIO pins** - Room for future expansion (NeoPixels, haptics)
2. **Dual-core processor** - Better performance and multitasking
3. **Built-in USB CDC** - No need for serial adapter
4. **More memory** - 264KB SRAM vs 2KB on Uno
5. **Better I2C support** - Hardware I2C with PIO fallback
6. **Native C SDK** - Better integration with modern tools

## Hardware Changes

### Remove
- Arduino Uno
- USB cable to Arduino
- Any Arduino shields/adapters

### Add
- Raspberry Pi Pico
- USB cable (Micro-B to USB-A/C)

### Keep
- Adafruit LED Arcade Button 1×4 QT I2C Breakout
- 4× 8×8 Matrix LED displays (HT16K33)
- All wiring and power connections

## Wiring Changes

### I2C Connections

**Old (Arduino):**
- SDA: A4
- SCL: A5

**New (Pico):**
- SDA: GPIO 4
- SCL: GPIO 5
- Add 4.7kΩ pull-up resistors on both lines to 3.3V if not already present

### USB Connection

**Old:** Arduino → Pi via USB
**New:** Pico → Pi via USB (Pico Micro-B port)

### Power

The Pico can be powered via USB from the Pi (5V → USB → Pico regulator → 3.3V).

**Important:** Ensure all I2C devices operate at 3.3V. The HT16K33 and NeoKey boards are 3.3V/5V compatible.

## Protocol Changes

### Commands (Pi → Device)

| Function | Old Arduino | New Pico |
|----------|-------------|----------|
| Menu State | `m\n` | `STATE 0\n` |
| Game State | `r\n` | `STATE 1\n` |
| Game Over | `g\n` | `STATE 2\n` |
| Set Score | `s=12345\n` | `SCORE 12345\n` |
| Set Balls | `b=3\n` | `BALLS 3\n` |

### Button State (Device → Pi)

**No changes** - Still single byte with bit flags:
- Bit 2: Left button
- Bit 1: Right button
- Bit 0: Center button

## Software Changes

### Pi Side

The `src/inputManagerPi.c` file has been updated to send the new protocol commands. No other Pi-side changes are needed.

**Key changes:**
```c
// Old
sprintf(tempString,"s=%ld\n",score);

// New
sprintf(tempString,"SCORE %ld\n",score);
```

### Pico Side

All new firmware in `/firmware` directory:
- Compiled with Pico SDK
- Flashed as `.uf2` file
- No Arduino IDE needed

## Migration Steps

### 1. Backup

Make a backup of your working system before making changes.

### 2. Physical Installation

1. Power off the system
2. Disconnect Arduino Uno
3. Rewire I2C to Pico (GPIO 4/5)
4. Connect Pico USB to Raspberry Pi

### 3. Build Pico Firmware

```bash
# On your development machine (not Pi)
cd firmware
export PICO_SDK_PATH=/path/to/pico-sdk
./build.sh
```

### 4. Flash Pico

1. Hold BOOTSEL button on Pico
2. Connect USB to your computer
3. Copy `firmware/build/pinball_firmware.uf2` to the Pico drive
4. Pico reboots automatically

### 5. Update Pi Software

```bash
# On Raspberry Pi
cd mini-pinball-machine
git pull  # Get updated inputManagerPi.c
cd src
# Rebuild your application
```

### 6. Test

1. Power on the system
2. Verify buttons respond
3. Check score display
4. Verify ball count display
5. Test state transitions

## Troubleshooting

### Pico not showing up as USB device

- Check USB cable (must support data, not just power)
- Try different USB port on Pi
- Reflash firmware using BOOTSEL mode

### I2C devices not working

- Check wiring: SDA to GPIO 4, SCL to GPIO 5
- Verify pull-up resistors (4.7kΩ to 3.3V)
- Check I2C addresses:
  - Arcade Seesaw: 0x3A
  - Matrices: 0x70, 0x71, 0x72, 0x73
- Voltage: Ensure all devices run at 3.3V

### Display shows garbage

- Check matrix addresses are sequential
- Verify column/row mapping in `hw_display.c`
- Test with simple pattern first

### Buttons not responding

- Check Arcade Seesaw at correct address (0x3A)
- Verify button index mapping
- Test I2C communication manually

### Serial not working

- Check `/dev/ttyACM0` exists on Pi
- Try different baud rate (though USB CDC ignores this)
- Check permissions: `sudo usermod -a -G dialout $USER`

## Testing I2C Devices

On Raspberry Pi, you can scan for I2C devices:

```bash
# Install i2c-tools if needed
sudo apt-get install i2c-tools

# Scan I2C bus
i2cdetect -y 1

# Should show:
# 0x3A - Arcade Seesaw button board
# 0x70, 0x71, 0x72, 0x73 - Matrix displays
```

## Rolling Back

If you need to revert to Arduino:

1. Disconnect Pico
2. Reconnect Arduino
3. Restore I2C wiring to Arduino pins (A4/A5)
4. Revert Git changes:
   ```bash
   git checkout HEAD~1 src/inputManagerPi.c
   ```
5. Rebuild Pi software

## Future Expansion

The Pico firmware is ready for Phase 2 & 3 features:

### Phase 2 (Available Soon)
- 65 NeoPixel LEDs
- Animated button LEDs
- Custom lighting effects

### Phase 3 (Planned)
- Haptic feedback motors
- Synchronized vibrations
- Enhanced gameplay feel

These features require additional hardware but the firmware stubs are already in place.

## Support

For issues or questions:
1. Check `PICO_FIRMWARE_SPEC.md` for detailed specification
2. Review `firmware/README.md` for build instructions
3. Open an issue on GitHub with:
   - Hardware setup description
   - I2C scan output
   - Pico firmware version
   - Error messages or symptoms
