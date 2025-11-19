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
- `STATE <state>` - Set game state
  - Text format (recommended): `STATE MENU`, `STATE GAME`, `STATE GAME_OVER`
  - Numeric format (legacy): `STATE 0` (MENU), `STATE 1` (GAME), `STATE 2` (GAME_OVER)
  - Automatically updates LED baseline patterns based on game state
- `EVENT <event>` - Trigger game events
  - `EVENT GAME_START` - Game starting (5x strobe on center button)
  - `EVENT BALL_READY` - Ball ready to launch (rapid blink on center button)
  - `EVENT BALL_LAUNCHED` - Ball launched (5x strobe on center button)
- `BTN_LED <idx> <mode> <r> <g> <b> [count]` - Direct button LED control (advanced)
  - `idx`: Button index (0=left, 1=center, 2=right)
  - `mode`: LED mode (0=off, 1=steady, 2=breathe, 3=blink, 4=strobe, 5=rapid_blink)
  - `r`, `g`, `b`: Color values (0-255)
  - `count`: Optional, for blink/strobe modes (default 1)
- Legacy commands (backwards compatibility):
  - `GAME_START` - Same as `EVENT GAME_START`
  - `BALL_LAUNCH` - Same as `EVENT BALL_LAUNCHED`

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

## LED Animation Specification

The Pico firmware owns all button LED animations. The Pi game sends high-level state changes and events; the Pico implements the animations.

### Menu State (`STATE MENU`)

**Left & Right Buttons:**
- Idle: Steady white
- On press: 2x fast strobe, then return to steady

**Center Button:**
- Idle: Fast breathe animation (smooth ramp up/down)
- On game start (`EVENT GAME_START`): 5x fast strobe, then transition to game state

### Gameplay State (`STATE GAME`)

**Left & Right Buttons:**
- Idle: Steady white
- On press: 1x fast strobe, then return to steady

**Center Button:**
- Normal gameplay: OFF
- Ball ready (`EVENT BALL_READY`): Rapid blink (continuous until launched)
- On launch (`EVENT BALL_LAUNCHED`): 5x fast strobe, then return to OFF

### Animation Details

- **Steady**: Constant brightness
- **Breathe**: Smooth brightness ramp (15ms steps, ~2 second cycle)
- **Strobe**: Very fast on/off (60ms per toggle)
- **Rapid Blink**: Continuous on/off (100ms per toggle)
- All animations use white color (255, 255, 255)
- Temporary effects (strobe) override baseline patterns
- After temporary effects complete, LEDs return to baseline pattern
- Ball ready state persists until `EVENT BALL_LAUNCHED` is received

### Technical Implementation

- Baseline patterns define the idle state for each button in each game state
- Active animations override baselines but don't modify them
- State tracking includes both game state and ball ready flag
- All timing handled in firmware main loop via `hw_button_leds_update()`
