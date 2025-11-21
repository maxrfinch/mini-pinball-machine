# Pinball Cabinet Firmware - Adafruit KB2040

This firmware controls all pinball cabinet hardware components via an Adafruit KB2040 microcontroller (RP2040-based).

## Hardware Components

### 1. NeoPixel LED Boards
- **48 total LEDs** (8 LEDs per board × 6 boards)
- **Single data line daisy chain** from GPIO6
- **Board positions** (player's perspective):
  - Board 1: Right Front (LEDs 0-7)
  - Board 2: Right Rear (LEDs 8-15)
  - Board 3: Camera Bar (LEDs 16-23)
  - Board 4: Left Rear (LEDs 24-31, reversed)
  - Board 5: Left Front (LEDs 32-39, reversed)
  - Board 6: Front Bar (LEDs 40-47)

### 2. Adafruit LED Arcade Button 1×4 I2C Breakout (Arcade Seesaw Board)
- **Three buttons**: LEFT, CENTER, RIGHT
- Connected via I²C0 bus (GPIO12/13 - STEMMA QT)
- I²C Address: 0x3A
- Product: Adafruit 5296

### 3. 1.2" 8×8 Matrix Displays with I2C Backpacks (4 Total)
- **4× Adafruit 1.2" 8x8 LED Matrix with HT16K33 backpack** (Adafruit 1855)
- Connected via I²C0 bus (GPIO12/13 - STEMMA QT), shared with arcade seesaw buttons
- Addresses: 0x70, 0x71, 0x72, 0x73
- Combined into 32×8 display surface

## GPIO Pinout

### NeoPixel Data
- **GPIO 6**: NeoPixel Data OUT

### I²C0 (STEMMA QT - Arcade Seesaw Buttons + Matrix Displays)
- **GPIO 12**: SDA0
- **GPIO 13**: SCL0

### Misc GPIO
- **GPIO 7**: Status LED (heartbeat)
- **GPIO 8**: Mode indicator
- **GPIO 22**: Optional BOOTSEL trigger

## Command Protocol

Commands are sent from the Raspberry Pi to the Pico via USB CDC serial. All commands are ASCII text, line-delimited.

### Commands TO Pico

```
CMD MODE <ATTRACT|MENU|GAME|BALL_LOST|HIGH_SCORE>
CMD SCORE <number>
CMD BALLS <number>
CMD EFFECT <pattern_name>
CMD BUTTON_EFFECT <effect_name>
CMD BRIGHTNESS <0-255>
CMD PING
```

**Examples:**
```
CMD MODE GAME
CMD SCORE 12345
CMD BALLS 3
CMD EFFECT RAINBOW_WAVE
CMD BUTTON_EFFECT READY_STEADY_GLOW
CMD BRIGHTNESS 128
CMD PING
```

### Events FROM Pico

```
EVT BUTTON <LEFT|CENTER|RIGHT> <DOWN|UP|HELD>
EVT PONG
EVT READY
EVT DEBUG ACTIVE
```

**Examples:**
```
EVT BUTTON LEFT DOWN
EVT BUTTON CENTER UP
EVT BUTTON RIGHT HELD
EVT PONG
EVT READY
EVT DEBUG ACTIVE
```

## NeoPixel LED Effects

The firmware includes 8 NeoPixel LED patterns:

1. **RAINBOW_BREATHE** - Slow rainbow brightness oscillation
2. **RAINBOW_WAVE** - Full-cabinet rainbow wave
3. **CAMERA_FLASH** - Only Board #3 flashes white, rest off
4. **RED_STROBE_5X** - Red strobe (5 flashes)
5. **WATER** - Aqua "flow" animation
6. **ATTRACT** - Idle attract-mode pattern (rainbow chase)
7. **PINK_PULSE** - Sharp pink pulse
8. **BALL_LAUNCH** - Left/right walls ripple front→back (yellow)

## Button LED Effects

The firmware includes 9 button LED effect modes for the arcade buttons:

1. **READY_STEADY_GLOW** - Idle/attract mode
   - Left/Right: Slow breathing fade (0→180→0), offset by 1 second
   - Center: Steady glow at 200 brightness
   - Use: Machine idle, attract mode, menu idle

2. **FLIPPER_FEEDBACK** - Button press confirmation
   - Fast pop to max brightness, immediate drop to 0, return to previous state
   - Duration: ~100ms
   - Use: Left/right flipper button presses

3. **CENTER_HIT_PULSE** - Launch ready indicator
   - Rapid double-strobe: 2 bursts at 40ms each, then 300ms dark
   - Loops continuously
   - Use: Center button when ball is ready to launch

4. **SKILL_SHOT_BUILDUP** - Pre-launch tension
   - All buttons ramp from 0→255 over 2 seconds, then snap to 0
   - Loops continuously
   - Use: Before launching ball, skill shot mode

5. **BALL_SAVED** - Multiball/save mode
   - Alternating flash left/right with decaying speed
   - 8 cycles over ~2 seconds, then returns to attract
   - Use: Temporary multiball or ball save mode

6. **POWERUP_ALERT** - Special game state
   - Chaotic strobe with randomized brightness
   - Duration: 1.5 seconds, then returns to attract
   - Use: Multiball start, jackpot, special moments

7. **EXTRA_BALL_AWARD** - Celebration
   - Three medium-speed pulses (200ms each)
   - One long fade-out over 1 second
   - Use: Extra ball award

8. **GAME_OVER_FADE** - Cooldown
   - All buttons: extremely slow fade (bright→off→bright, 2.5s cycle)
   - Loops continuously
   - Use: Game over state

9. **MENU_NAVIGATION** - Menu mode
   - Selected button: 255 brightness
   - Other buttons: 80 brightness
   - Use: Menu navigation mode

## Debug Mode

When no commands are received for 30 seconds, the firmware enters debug mode:

1. **Button LEDs**: Left slow pulse, Center steady, Right fast pulse
2. **NeoPixels**: Board-ID chase, global rainbow, reverse-direction test
3. **8×8 Matrices**: Test patterns (digits, ball icons, scrolling)
4. **Serial output**: `EVT DEBUG ACTIVE`

Debug mode exits immediately upon receiving any command.

## Diagnostics and Troubleshooting

The firmware includes comprehensive diagnostic logging to help identify hardware issues. All diagnostic messages are sent via USB serial (115200 baud).

### Startup Diagnostics

On boot, the firmware logs:
- System banner and version
- I²C bus initialization (I2C0, I2C1)
- Device detection and configuration status
- GPIO pin assignments

### Debug Mode Self-Test

When debug mode activates, a comprehensive I²C bus self-test runs automatically:
- Tests I2C0 (Seesaw buttons at 0x3A, Matrix displays at 0x70-0x73)
- Reports pass/fail status for each device

### Runtime Monitoring

During operation:
- Button press/release events are logged
- I²C communication failures are tracked
- Raw GPIO states logged periodically
- Display ACK/NACK status reported

### Complete Documentation

See **[DIAGNOSTICS.md](DIAGNOSTICS.md)** for:
- Complete message reference
- Troubleshooting procedures
- Expected output examples
- Hardware debugging guide

## Building the Firmware

### Prerequisites

1. **Raspberry Pi Pico SDK** - Install from https://github.com/raspberrypi/pico-sdk
2. **CMake** (version 3.13 or later)
3. **GCC ARM toolchain** (arm-none-eabi-gcc)
4. **Build tools** (make, ninja, etc.)

### Installation on Linux/macOS

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Clone Pico SDK
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init
export PICO_SDK_PATH=$(pwd)

# Add to ~/.bashrc or ~/.zshrc for persistence
echo "export PICO_SDK_PATH=$(pwd)" >> ~/.bashrc
```

### Build Steps

```bash
cd firmware
./build.sh
```

Or manually:

```bash
cd firmware
mkdir build
cd build
cmake ..
make -j4
```

The output file will be: `build/pinball_firmware.uf2`

## Flashing the Firmware

1. **Hold BOOTSEL button** on the Adafruit KB2040
2. **Connect USB cable** to your computer
3. The KB2040 will appear as a USB mass storage device
4. **Copy** `build/pinball_firmware.uf2` to the KB2040 drive
5. The KB2040 will **automatically reboot** and start running the firmware

## Testing

### Without Hardware

The firmware will compile but cannot be fully tested without hardware components. Missing I²C devices will cause initialization to fail gracefully.

### With Hardware

1. Flash firmware to KB2040
2. Connect all I²C devices to STEMMA QT connector (GP12/GP13)
3. Connect NeoPixel strip to GPIO6
4. Connect USB to Raspberry Pi
5. The KB2040 will send `EVT READY` on startup
6. Press buttons to verify events are sent
7. Send commands via serial to test functionality

### Debug Mode Testing

1. Flash firmware and power up
2. Wait 30 seconds without sending commands
3. Observe debug animations on all hardware
4. Serial output should show `EVT DEBUG ACTIVE`
5. Send any command to exit debug mode

## Serial Communication

The firmware uses USB CDC for serial communication at native USB speeds. No baud rate configuration is needed.

### On Linux/macOS:
```bash
# Find the device
ls /dev/tty*

# Connect with screen
screen /dev/ttyACM0

# Or use minicom
minicom -D /dev/ttyACM0
```

### On Windows:
Use PuTTY or TeraTerm to connect to the Pico's COM port.

## Troubleshooting

### Build Errors

**Error: "PICO_SDK_PATH not set"**
- Set the environment variable: `export PICO_SDK_PATH=/path/to/pico-sdk`

**Error: "arm-none-eabi-gcc not found"**
- Install ARM toolchain: `sudo apt install gcc-arm-none-eabi`

### Hardware Issues

**NeoPixels not working:**
- Check GPIO6 connection
- Verify 5V power supply for LEDs
- Add a 330Ω resistor in series with data line
- Add a 1000µF capacitor across power supply

**I²C devices not responding:**
- Verify STEMMA QT connection (GP12/GP13)
- Add 4.7kΩ pull-up resistors on SDA and SCL lines if not using STEMMA QT
- Check I²C addresses with i2c-tools
- Ensure proper 3.3V power supply

**Buttons not detected:**
- Verify Arcade Seesaw board at address 0x3A
- Check I²C0 STEMMA QT connection
- Test with i2cdetect utility

### Debug Mode Not Activating

- Ensure 30 seconds have passed without commands
- Check that commands are properly terminated with newline
- Verify USB CDC connection is active

## File Structure

```
firmware/
├── CMakeLists.txt          # CMake build configuration
├── build.sh                # Build script
├── README.md               # This file
├── include/                # Header files
│   ├── hardware_config.h   # GPIO and I2C configuration
│   ├── types.h             # Common type definitions
│   ├── protocol.h          # Command protocol
│   ├── neopixel.h          # NeoPixel driver
│   ├── buttons.h           # Button driver
│   ├── display.h           # Matrix display driver
│   └── debug_mode.h        # Debug mode
└── src/                    # Source files
    ├── main.c              # Main entry point
    ├── protocol.c          # Command parser
    ├── neopixel.c          # NeoPixel implementation
    ├── buttons.c           # Button implementation
    ├── display.c           # Display implementation
    ├── debug_mode.c        # Debug mode implementation
    └── ws2812.pio          # PIO program for WS2812 LEDs
```

## Future Enhancements

- Configurable LED effect parameters

- Adjustable button debounce timing
- Display brightness control
- EEPROM storage for settings
- Bootloader for OTA updates

## License

This firmware is part of the mini-pinball-machine project.

## Credits

Based on the WS2812 PIO example from the Raspberry Pi Pico SDK.
