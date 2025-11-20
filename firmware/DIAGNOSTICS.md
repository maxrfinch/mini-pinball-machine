# Firmware Diagnostics and Logging

This document describes the diagnostic logging added to help identify I²C bus and device communication issues.

## Overview

Comprehensive logging has been added to all I²C drivers to help identify why certain buses or devices may not be responding. The firmware will now report detailed information over USB serial during initialization and operation.

## Architecture Summary

The firmware uses two hardware I²C buses:

1. **I2C0 (Hardware I²C)** - GPIO4/5
   - Adafruit Arcade Seesaw Button Board (0x3A)
   - HT16K33 Matrix Display 0 (0x70)
   - HT16K33 Matrix Display 1 (0x71)
   - HT16K33 Matrix Display 2 (0x72)
   - HT16K33 Matrix Display 3 (0x73)
   
2. **I2C1 (Hardware I²C)** - GPIO6/7
   - DRV2605L Left Haptic (0x5A)
   - DRV2605L Right Haptic (0x5B)

## Startup Logging

### System Banner
```
╔═══════════════════════════════════════════════════════════╗
║        Raspberry Pi Pico Pinball Controller v1.0         ║
╚═══════════════════════════════════════════════════════════╝
```

### Initialization Sequence

Each subsystem logs its initialization:

#### NeoPixels
```
NeoPixels initialized
```

#### Buttons (I2C0)
```
=== Button Initialization (I2C0 - Hardware I2C) ===
Initializing I2C0 at 100000 Hz on GPIO4 (SDA) / GPIO5 (SCL)
I2C0 hardware initialized
Configuring Seesaw at address 0x3A...
  Button direction configured
  Button pull-ups enabled
=== Button Initialization Complete ===
```

**Error Indicators:**
- `WARNING: Failed to set button direction (Seesaw not responding?)` - I²C write failed
- `WARNING: Failed to enable button pull-ups` - I²C write failed

#### Haptics (I2C1)
```
=== Haptics Initialization (I2C1 - Hardware I2C) ===
Initializing I2C1 at 100000 Hz on GPIO6 (SDA) / GPIO7 (SCL)
I2C1 hardware initialized
Initializing DRV2605L at 0x5A...
  DRV2605L 0x5A initialized successfully
Initializing DRV2605L at 0x5B...
  DRV2605L 0x5B initialized successfully
=== Haptics Initialization Complete ===
```

**Error Indicators:**
- `Failed to exit standby mode` - Device not responding
- `Failed to set trigger mode` - Communication error
- `Failed to select library` - Communication error
- `Failed to configure control` - Communication error

#### Displays (Shared I2C0 Hardware Bus)
```
=== Display Initialization (Shared I2C0 Hardware Bus) ===
Matrix displays share I2C0 with Seesaw buttons
I2C0 already initialized at 100000 Hz on GPIO4 (SDA) / GPIO5 (SCL)
Initializing 4 HT16K33 matrix displays on I2C0...
Initializing HT16K33 at 0x70...
  HT16K33 0x70 initialized successfully
Initializing HT16K33 at 0x71...
  HT16K33 0x71 initialized successfully
Initializing HT16K33 at 0x72...
  HT16K33 0x72 initialized successfully
Initializing HT16K33 at 0x73...
  HT16K33 0x73 initialized successfully
=== Display Initialization Complete ===
```

**Error Indicators:**
- `HT16K33 0xXX: I2C write failed (wrote N/M bytes)` - Device not responding or communication error
- `Failed to turn on oscillator` - Device communication failed
- `Failed to set brightness` - Device communication failed
- `Failed to turn on display` - Device communication failed

## Runtime Logging

### Button Polling

The button driver continuously polls the Seesaw board and logs:

#### First Successful Read
```
SEESAW: First successful read on i2c0
```

#### Periodic Status (every 500 polls ≈ 5 seconds)
```
SEESAW: raw GPIO = 0x00000007 (poll #500)
SEESAW: raw GPIO = 0x00000007 (poll #1000)
```

The raw GPIO value shows the state of all button pins (buttons are active-low).

#### Read Errors
```
SEESAW: read FAILED on i2c0 (error #1)
SEESAW: read FAILED on i2c0 (error #2)
...
SEESAW: read FAILED on i2c0 (error #100)
```

First 5 errors are logged individually, then every 100th error to avoid log spam.

#### Button Events
```
BUTTON LEFT: PRESSED
BUTTON LEFT: RELEASED
BUTTON CENTER: PRESSED
BUTTON CENTER: RELEASED
BUTTON RIGHT: PRESSED
BUTTON RIGHT: RELEASED
```

### Display Updates

The display driver logs ACK/NACK for every write operation:

```
HT16K33 0x70: NO ACK on address
HT16K33 0x71: NO ACK on data[5] = 0xFF
```

These messages appear when displays fail to acknowledge I²C transactions.

## Debug Mode

### Entry
After 30 seconds of inactivity, debug mode automatically activates:

```
*** ENTERING DEBUG MODE ***
```

### Self-Test

When debug mode starts, a comprehensive bus self-test runs:

```
╔══════════════════════════════════════════════════════════════╗
║              DEBUG MODE - I2C BUS SELF-TEST                  ║
╚══════════════════════════════════════════════════════════════╝

┌─ I2C0 Bus (Hardware) ─────────────────────────────────────┐
│ GPIOs: 4 (SDA), 5 (SCL)
│ Frequency: 100000 Hz
│
│ Testing Seesaw (0x3A)... ✓ OK - Device responding
└────────────────────────────────────────────────────────────┘

┌─ I2C1 Bus (Hardware) ─────────────────────────────────────┐
│ GPIOs: 6 (SDA), 7 (SCL)
│ Frequency: 100000 Hz
│
│ Testing DRV2605L Left (0x5A)... ✓ OK
│ Testing DRV2605L Right (0x5B)... ✓ OK
└────────────────────────────────────────────────────────────┘

┌─ Matrix Displays (Shared on I2C0) ────────────────────────┐
│ Note: Matrices share I2C0 hardware bus with Seesaw
│
│ Testing HT16K33 Matrix 0 (0x70)... ✓ OK
│ Testing HT16K33 Matrix 1 (0x71)... ✓ OK
│ Testing HT16K33 Matrix 2 (0x72)... ✓ OK
│ Testing HT16K33 Matrix 3 (0x73)... ✓ OK
└────────────────────────────────────────────────────────────┘

Self-test complete. Monitor above for any failures.
Button presses and display updates will be logged during debug mode.
```

**Pass/Fail Indicators:**
- `✓ OK` - Device is responding
- `✗ FAILED` - Device is not responding

### Exit
Debug mode exits when any command is received:

```
*** EXITING DEBUG MODE ***
```

## Troubleshooting Guide

### Problem: "Seesaw not responding" during initialization

**Possible Causes:**
1. Incorrect wiring - check GPIO4 (SDA) and GPIO5 (SCL)
2. Missing pull-up resistors (4.7kΩ required)
3. Wrong Seesaw address (should be 0x3A)
4. Insufficient power to Seesaw board
5. Defective Seesaw board

**Diagnostic Steps:**
1. Check for "I2C0 hardware initialized" message
2. Look for "WARNING: Failed to set button direction"
3. Run debug mode and check self-test results
4. Verify no "SEESAW: read FAILED" messages appear

### Problem: "HT16K33 I2C write failed"

**Possible Causes:**
1. Incorrect address configuration (solder jumpers)
2. Wiring issue on GPIO4 (SDA) or GPIO5 (SCL)
3. Missing pull-up resistors (4.7kΩ required)
4. Power supply issue
5. Defective display backpack
6. I2C bus contention with Seesaw

**Diagnostic Steps:**
1. Verify "I2C0 already initialized" message during display init
2. Check which displays initialize successfully during init
3. Verify address jumpers on non-responding displays (0x70-0x73)
4. Check pull-ups on GPIO4/5
5. Run debug mode self-test to probe all I2C0 devices

### Problem: "DRV2605L initialization failed"

**Possible Causes:**
1. Wrong I2C address (left should be 0x5A, right 0x5B)
2. Wiring issue on GPIO6 (SDA) or GPIO7 (SCL)
3. Missing pull-up resistors (4.7kΩ required)
4. Power supply issue
5. Defective DRV2605L board

**Diagnostic Steps:**
1. Check for "I2C1 hardware initialized" message
2. Look for specific error messages (standby, trigger, library, control)
3. Run debug mode self-test
4. Verify haptics work (should buzz every 10 seconds in debug mode)

### Problem: No button events logged when pressing buttons

**Possible Causes:**
1. Seesaw communication is failing (see above)
2. Buttons are not physically connected to Seesaw
3. Wrong button mapping in hardware

**Diagnostic Steps:**
1. Check "SEESAW: First successful read on i2c0" appears
2. Monitor "SEESAW: raw GPIO" values - they should change when buttons pressed
3. Buttons are active-low, so GPIO bits should clear when pressed
4. Press each button and watch for raw GPIO changes

### Problem: Displays don't light up

**Possible Causes:**
1. Bit-bang I²C communication failing (see HT16K33 section)
2. Display not initialized properly
3. Framebuffer not being updated
4. Power issue to displays

**Diagnostic Steps:**
1. Check all 4 displays initialized successfully
2. Look for "NO ACK" messages during updates
3. In debug mode, displays should show test patterns
4. Verify display_update() is being called

## USB Serial Connection

To view these diagnostic messages:

### Linux/macOS
```bash
screen /dev/ttyACM0 115200
# or
minicom -D /dev/ttyACM0 -b 115200
```

### Windows
Use PuTTY, TeraTerm, or Arduino Serial Monitor:
- Port: COMx (check Device Manager)
- Baud: 115200
- Data bits: 8
- Stop bits: 1
- Parity: None

### Note
Wait 1 second after connecting USB for the device to enumerate before the banner appears.

## Summary of Log Messages

| Message | Location | Meaning | Action |
|---------|----------|---------|--------|
| `Button direction configured` | buttons.c init | Seesaw responded to configuration | ✓ Good |
| `WARNING: Failed to set button direction` | buttons.c init | Seesaw not responding on I2C0 | Check wiring, address, power |
| `SEESAW: First successful read` | buttons.c poll | First successful button poll | ✓ Good |
| `SEESAW: read FAILED` | buttons.c poll | Button polling failed | Check I2C0 connection |
| `BUTTON X: PRESSED/RELEASED` | buttons.c poll | Button event detected | ✓ Good |
| `HT16K33 0xXX initialized successfully` | display.c init | Display acknowledged commands | ✓ Good |
| `HT16K33 0xXX: NO ACK on address` | display.c write | Display not responding | Check address jumpers, wiring |
| `DRV2605L 0xXX initialized successfully` | haptics.c init | Haptic driver ready | ✓ Good |
| `Failed to exit standby mode` | haptics.c init | Haptic not responding on I2C1 | Check wiring, address, power |
| `Testing X... ✓ OK` | debug_mode.c | Device responds to probe | ✓ Good |
| `Testing X... ✗ FAILED` | debug_mode.c | Device does not respond | Check connection |

## Code Changes Summary

All changes are additive (logging only). No functional behavior was modified:

- **display.c**: Added 35 lines of logging
- **buttons.c**: Added 53 lines of logging
- **haptics.c**: Added 15 lines of logging
- **debug_mode.c**: Added 87 lines of logging and self-test
- **main.c**: Added 26 lines of startup logging

Total: 216 lines added, 6 lines removed (cleanup)
