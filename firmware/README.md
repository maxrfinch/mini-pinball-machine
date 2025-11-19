# Pico Pinball Firmware

This directory contains the Raspberry Pi Pico firmware for the mini pinball machine.

## Hardware Requirements

- Raspberry Pi Pico
- Adafruit LED Arcade Button 1×4 QT I2C Breakout (I2C address 0x30)
- 4× 8×8 Matrix LED displays with HT16K33 drivers (I2C addresses 0x70-0x73)

## Pin Configuration

### I2C0
- SDA: GPIO 4
- SCL: GPIO 5

### USB CDC
- Used for serial communication with Raspberry Pi 4/5

## Building

1. Install the Pico SDK and set `PICO_SDK_PATH` environment variable
2. Create a build directory:
   ```bash
   cd firmware
   mkdir build
   cd build
   ```
3. Configure and build:
   ```bash
   cmake ..
   make
   ```
4. Flash `pinball_firmware.uf2` to the Pico by holding BOOTSEL while connecting USB

## Serial Protocol

### Pi → Pico Commands (ASCII, newline-terminated)

- `SCORE <value>` - Update score display (e.g., `SCORE 12345`)
- `BALLS <count>` - Update ball indicators (e.g., `BALLS 3`)
- `STATE <id>` - Set game state (0=MENU, 1=GAME, 2=GAME_OVER)
  - Automatically updates LED baseline patterns based on game state
- `BTN_LED <idx> <mode> <r> <g> <b> [count]` - Control button LEDs
  - `idx`: Button index (0=left, 1=center, 2=right)
  - `mode`: LED mode (0=off, 1=steady, 2=breathe, 3=blink, 4=strobe)
  - `r`, `g`, `b`: Color values (0-255)
  - `count`: Optional, for blink/strobe modes (default 1)
- `GAME_START` - Trigger game start LED effect (blinks center button 2x)
- `BALL_LAUNCH` - Trigger ball launch LED effect (strobes center button 3x)

### Pico → Pi Messages

- Single byte representing button state:
  - Bit 2: Left button
  - Bit 1: Right button
  - Bit 0: Center button

## Phase 1 Implementation Status

- [x] USB CDC/UART serial communication
- [x] QT I2C button polling
- [x] 32×8 matrix display system
- [x] Score rendering (right-aligned, rows 0-4)
- [x] Ball count rendering (2×2 blocks, rows 6-7)
- [x] Command parser (SCORE, BALLS, STATE)
- [x] Main event loop
- [x] Button LED control with animations (breathe, blink, strobe)
- [x] BTN_LED command implementation
- [x] LED game state logic with baseline patterns
- [x] Button press LED feedback
- [x] Game event LED effects (game start, ball launch)

## Future Phases (TODO)

### Phase 2
- NeoPixel control (65 LEDs)
- Commands: `NEO_SET`, `NEO_FILL`, `NEO_MODE`, `NEO_POWER_MODE`

### Phase 3
- Haptic motor control (left/right)
- Command: `HAPT`

## Architecture

```
/firmware
  /src
    main.c           - Main event loop, button press detection
    protocol.c       - Command parser
    hw_serial.c      - USB CDC communication
    hw_buttons.c     - I2C button polling
    hw_display.c     - Matrix display rendering
    hw_button_leds.c - Button LED control and game-aware patterns
  CMakeLists.txt     - Build configuration
  README.md          - This file
```

## LED Game State Logic

The firmware now includes game-aware LED control with baseline patterns:

### Game States
- **MENU**: Left/right steady white, center breathing white
- **IN_GAME**: All buttons steady white
- **GAME_OVER**: Same as MENU

### Automatic Behaviors
- Button presses trigger a quick blink for tactile feedback
- Game start triggers center button blink (2x)
- Ball launch triggers center button strobe (3x)
- STATE command automatically applies appropriate baseline patterns

### Technical Details
- All LEDs use white-only mode (255, 255, 255)
- Baseline patterns are maintained separately from temporary effects
- Temporary effects (blink/strobe) automatically return to OFF when complete
- Button press detection in main loop triggers LED feedback
