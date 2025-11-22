# Arcade Pinball Controller Firmware – Full Consolidated Documentation (Version A)

## Table of Contents
1. System Overview
2. Controller State Machine
3. Hardware Architecture
4. Wiring & Setup
5. Build & Flash Instructions
6. Host Communication Protocol
   - 6.1. State Commands
   - 6.2. Event Commands
   - 6.3. Override Commands
   - 6.4. Events (KB2040 → Pi)
   - 6.5. Menu Behavior
   - 6.6. Game Flow Signals
   - 6.7. Complete Game Flow Examples (NEW)
7. Button Input & LED Effects (Full Reference)
8. Matrix Display System
9. NeoPixel Lighting System
10. Debug Mode & Diagnostics (Consolidated)
11. Troubleshooting Guide
12. Implementation Summary
13. Project Structure
14. Customization Guide
15. Appendix: Recommended Game‑State Mapping

---

# 1. System Overview
This firmware powers an **arcade‑style pinball controller** built around an **Adafruit KB2040**.  
It provides:

- **Three arcade buttons** using an Adafruit Seesaw breakout (LEFT, CENTER, RIGHT)
- **Four 8×8 LED matrices** combined into a 32×8 score/ball display
- **A 48‑LED NeoPixel lighting system**
- **A clean ASCII control protocol** for communication with a Raspberry Pi running the pinball game
- **Full diagnostics**, debug mode, and hardware abstraction

The microcontroller manages *all cabinet hardware* so the host game only sends simple commands.

## Recent Updates
- **PR #37**: Fixed button LED animation timing and synchronization issues. Button effects now properly respect priority system and transition smoothly between states.

---

# 2. Controller State Machine

The firmware implements a central state machine that manages the controller's mode, visual behavior, and button interactions. This allows the KB2040 to own more of the visual/feel logic while the Pi sends high-level game state and events.

## 2.1. Controller Modes

The controller operates in one of six top-level modes:

| Mode | Description | Default Visual Behavior |
|------|-------------|------------------------|
| **ATTRACT** | Idle/demo mode | NeoPixels: ATTRACT effect<br>Buttons: READY_STEADY_GLOW |
| **MENU** | Menu navigation | NeoPixels: ATTRACT effect<br>Buttons: MENU_NAVIGATION |
| **GAME** | Active gameplay | Depends on ball state (see substates) |
| **BALL_LOST** | Ball drain sequence | NeoPixels: RED_STROBE_5X<br>Buttons: READY_STEADY_GLOW |
| **HIGH_SCORE** | High score entry/display | NeoPixels: PINK_PULSE<br>Buttons: READY_STEADY_GLOW |
| **DEBUG** | Diagnostic mode | Special debug patterns |

## 2.2. Controller Substates

Within modes, the controller tracks finer-grained states:

| Substate | Used In Mode | Meaning |
|----------|--------------|---------|
| **SUB_NONE** | Multiple | Default/unspecified |
| **SUB_MENU_IDLE** | MENU | Menu displayed, no active navigation |
| **SUB_MENU_NAV** | MENU | User actively navigating menu |
| **SUB_BALL_READY** | GAME | Ball loaded, ready to launch |
| **SUB_BALL_IN_PLAY** | GAME | Ball actively in play |
| **SUB_SKILL_SHOT** | GAME | Skill shot opportunity active |
| **SUB_MULTIBALL** | GAME | Multiball mode active |

## 2.3. Game State Flags

The controller tracks several boolean flags that affect behavior:

- **ball_ready**: When true in GAME mode, indicates ball is ready to launch
  - Activates BALL_LAUNCH effect on NeoPixels
  - Activates CENTER_HIT_PULSE on center button
  - Center button press triggers ball launch (host detects from raw button event)

- **skill_shot_active**: Indicates skill shot window is active
  - Can be used to modify visual effects during launch

- **multiball_active**: Indicates multiball mode is active
  - May affect base visual profiles in future enhancements

## 2.4. Menu State

For menu navigation, the controller maintains:

- **menu_index**: Current selected menu item (0-based)
- **menu_count**: Total number of menu items
- **menu_smart**: Navigation mode
  - `true` (SMART): Controller manages index, emits MENU_MOVE/MENU_SELECT events
  - `false` (DUMB): Host manages index, controller only handles visuals

## 2.5. Effect Priority System

Visual effects operate on a three-level priority system:

| Priority | Value | Description | Use Cases |
|----------|-------|-------------|-----------|
| **BASE** | 0 | Long-lived defaults | Mode-driven base visuals |
| **EVENT** | 1 | Short-lived events | Jackpot, ball save, extra ball (1-3s) |
| **CRITICAL** | 2 | Manual overrides | Debug commands, error conditions |

Priority rules:
- Lower priority effects cannot override higher priority effects
- EVENT priority effects automatically revert to BASE after their duration expires
- CRITICAL priority persists until explicitly cleared
- Both NeoPixels and button LEDs have independent priority tracking

---

# 3. Hardware Architecture

### Microcontroller
- **Adafruit KB2040** (RP2040 MCU, USB‑native, STEMMA QT I²C)

### External Devices
| Component | Qty | Description | Bus |
|----------|-----|-------------|-----|
| Arcade buttons | 3 | LEFT, CENTER, RIGHT | I²C via Seesaw |
| Seesaw arcade board | 1 | Adafruit 5296 | I²C @ 0x3A |
| LED matrix (1.2” HT16K33) | 4 | Score/ball display | I²C @ 0x70–0x73 |
| NeoPixel boards | 6 | 8 LEDs each | GPIO6, PIO0 |
| Status LEDs | 2 | Heartbeat / Mode indicators | GPIO7/GPIO8 |

### GPIO Summary
- **GPIO6** – NeoPixel data  
- **GPIO17** – Onboard KB2040 NeoPixel  
- **GPIO12/13** – I²C0 (SDA/SCL) via STEMMA QT  
- **GPIO7/8** – Optional indicator LEDs  
- **GPIO22** – Optional software‑controlled BOOTSEL

---

# 4. Wiring & Setup

### NeoPixels
- Data: **GPIO6 → 330Ω resistor → first LED board**
- Power: 5V (≥3A recommended)
- 1000 µF capacitor across 5V and GND
- Boards:
  - Board 1: Right Front (0–7)
  - Board 2: Right Rear (8–15)
  - Board 3: Camera Bar (16–23)
  - Board 4: Left Rear (24–31, reversed)
  - Board 5: Left Front (32–39, reversed)
  - Board 6: Front Bar (40–47)

### I²C0 (GPIO12/13)
All devices daisy‑chain via STEMMA QT:
- **0x3A** – Seesaw arcade board  
- **0x70–0x73** – HT16K33 matrix backpacks  

### Button Inputs
Buttons connect to Seesaw:
- BTN0 → LEFT
- BTN1 → CENTER
- BTN2 → RIGHT

### Display Address Jumpers
| Display | Address | Jumpers |
|--------|---------|---------|
| 1 | 0x70 | none |
| 2 | 0x71 | A0 |
| 3 | 0x72 | A1 |
| 4 | 0x73 | A0 + A1 |

---

# 5. Build & Flash Instructions

### Requirements
- Raspberry Pi Pico SDK  
- ARM GCC toolchain (`arm-none-eabi-gcc`)  
- CMake 3.13+  

### Build
```bash
cd firmware
./build.sh
```

Output UF2:
```
build/pinball_firmware.uf2
```

### Flash
1. Hold **BOOTSEL**
2. Plug in USB
3. Copy UF2 onto mounted drive

---

# 6. Host Communication Protocol

ASCII messages over USB CDC, newline‑terminated. The protocol is organized into three categories: state commands (long-lived configuration), event commands (short-lived events), and override commands (manual control).

## 6.1. State Commands (Pi → KB2040)

Long-lived configuration and game state. These commands set the controller's persistent state and trigger base profile updates.

### Mode Control
```
CMD MODE <ATTRACT|MENU|GAME|BALL_LOST|HIGH_SCORE|DEBUG>
```
Sets the top-level controller mode. Automatically applies the appropriate base visual profile for the new mode.

**Examples:**
```
CMD MODE ATTRACT        # Enter attract/demo mode
CMD MODE GAME           # Start active gameplay
CMD MODE MENU           # Enter menu navigation
CMD MODE BALL_LOST      # Ball drain sequence
CMD MODE HIGH_SCORE     # High score entry
```

### Game State Flags
```
CMD STATE BALL_READY <0|1>
CMD STATE SKILL_SHOT <0|1>
CMD STATE MULTIBALL <0|1>
```

- `BALL_READY 1`: Ball is loaded and ready to launch (activates launch-ready visuals)
- `BALL_READY 0`: Ball is in play or not available
- `SKILL_SHOT 1`: Skill shot opportunity window is active
- `MULTIBALL 1`: Multiple balls in play

**Example sequence:**
```
CMD STATE BALL_READY 1    # Ball loaded, show launch prompt
# (Player presses center button)
# Controller emits: EVT BUTTON CENTER DOWN
# Controller automatically clears ball_ready flag
# Host detects launch from button event while in ball_ready state
```

### Menu Configuration
```
CMD MENU_MODE <SMART|DUMB>
CMD MENU_SIZE <n>
CMD MENU_INDEX <i>
```

- `MENU_MODE SMART`: Controller manages menu_index internally, emits navigation events
- `MENU_MODE DUMB`: Host manages menu_index, controller only handles visuals
- `MENU_SIZE`: Total number of menu items (for wrap-around)
- `MENU_INDEX`: Set current selection (DUMB mode) or initial selection (SMART mode)

### Score & Display
```
CMD SCORE <number>
CMD BALLS <number>
CMD TEXT <x> <y> <text>
CMD DISPLAY_ANIM <animation_name>
```

- `SCORE`: Updates the LED matrix display with current score
- `BALLS`: Updates the ball count display
- `TEXT`: Displays custom text at position (x, y) on the 32×8 matrix
  - x: horizontal position (0-31, origin at left)
  - y: vertical position (0-7, origin at top)
  - Note: Clears display before drawing text
- `DISPLAY_ANIM`: Starts a display animation (NONE, BALL_SAVED, MULTIBALL, MAIN_MENU)

### Brightness
```
CMD BRIGHTNESS <0-255>
```

Sets global NeoPixel brightness. Does not affect button LEDs.

### System Commands
```
CMD PING
CMD DEBUG
```

- `PING`: Controller responds with `EVT PONG`
- `DEBUG`: Enter debug/diagnostic mode

---

## 6.2. Event Commands (Pi → KB2040)

Short-lived game events that trigger temporary visual effects at EVENT priority. Effects automatically revert to base profile after their duration expires.

```
CMD EVENT BALL_SAVED
CMD EVENT EXTRA_BALL
CMD EVENT JACKPOT
CMD EVENT MULTIBALL_START
CMD EVENT MULTIBALL_END
```

| Event | NeoPixel Effect | Button Effect | Duration |
|-------|----------------|---------------|----------|
| BALL_SAVED | RED_STROBE_5X | BALL_SAVED | 1.5s |
| EXTRA_BALL | PINK_PULSE | EXTRA_BALL_AWARD | 2.0s |
| JACKPOT | RAINBOW_WAVE | POWERUP_ALERT | 2.5s |
| MULTIBALL_START | PINK_PULSE | POWERUP_ALERT | 2.0s |
| MULTIBALL_END | *(reverts to base)* | *(reverts to base)* | immediate |

**When to send:**
- `BALL_SAVED`: Ball save feature activates, preventing drain
- `EXTRA_BALL`: Player awarded an extra ball
- `JACKPOT`: Major scoring event achieved
- `MULTIBALL_START`: Multiball mode begins (also sets multiball_active flag)
- `MULTIBALL_END`: Multiball mode ends (clears multiball_active flag)

---

## 6.3. Override Commands (Pi → KB2040)

Manual control at CRITICAL priority. Overrides persist until explicitly cleared, blocking all base and event updates.

**Intended use:** Debug sequences, special modes, testing.

### NeoPixel Override
```
CMD EFFECT_OVERRIDE <EFFECT_NAME>
CMD EFFECT_CLEAR
```

Effect names: RAINBOW_BREATHE, RAINBOW_WAVE, CAMERA_FLASH, RED_STROBE_5X, WATER, ATTRACT, PINK_PULSE, BALL_LAUNCH, NONE

**Example:**
```
CMD EFFECT_OVERRIDE WATER     # Force water effect
# (mode changes, events, etc. will not change NeoPixels)
CMD EFFECT_CLEAR              # Release override, return to base profile
```

### Button LED Override
```
CMD BUTTON_EFFECT_OVERRIDE <EFFECT_NAME>
CMD BUTTON_EFFECT_CLEAR
```

Effect names: OFF, READY_STEADY_GLOW, FLIPPER_FEEDBACK, CENTER_HIT_PULSE, SKILL_SHOT_BUILDUP, BALL_SAVED, POWERUP_ALERT, EXTRA_BALL_AWARD, GAME_OVER_FADE, MENU_NAVIGATION

---

## 6.4. Events (KB2040 → Pi)

**Important:** The controller only sends button-originated events back to the Pi. No autonomous telemetry or game-state-derived events are sent.

### Button Events (Always Sent)
```
EVT BUTTON <LEFT|CENTER|RIGHT> <DOWN|UP|HELD>
```

Raw button state changes:
- `DOWN`: Button pressed
- `UP`: Button released
- `HELD`: Button held for >500ms

**Example:**
```
EVT BUTTON LEFT DOWN
EVT BUTTON LEFT UP
EVT BUTTON CENTER HELD
```

### Menu Events (SMART Mode Only)
```
EVT MENU_MOVE <index>
EVT MENU_SELECT <index>
```

Sent when `MENU_MODE SMART` is active and player navigates menu:
- `MENU_MOVE`: Player pressed LEFT/RIGHT, index updated (with wrap-around)
- `MENU_SELECT`: Player pressed CENTER, selecting current item

**Example flow:**
```
# Host:
CMD MODE MENU
CMD MENU_MODE SMART
CMD MENU_SIZE 5
CMD MENU_INDEX 0

# Player presses RIGHT:
EVT BUTTON RIGHT DOWN        # Raw button event
EVT MENU_MOVE 1              # Menu semantic event

# Player presses CENTER:
EVT BUTTON CENTER DOWN       # Raw button event
EVT MENU_SELECT 1            # Menu semantic event
```

### Game Flow - Ball Launch Detection

The controller handles ball launch internally but does not emit a separate launch event. The host detects ball launch by monitoring raw button events.

**Example flow:**
```
# Host:
CMD MODE GAME
CMD STATE BALL_READY 1

# Controller shows ball-ready visuals (BALL_LAUNCH effect, CENTER_HIT_PULSE)

# Player presses CENTER:
EVT BUTTON CENTER DOWN       # Raw button event (only event sent)

# Controller automatically (internal to controller):
# - Sets ball_ready = false
# - Changes substate to SUB_BALL_IN_PLAY
# - Reverts to in-play base profile

# Host detects launch by receiving CENTER DOWN while ball_ready was true
```

### System Events
```
EVT READY
EVT PONG
EVT DEBUG ACTIVE
```

- `READY`: Sent once at startup when controller is initialized
- `PONG`: Response to `CMD PING`
- `DEBUG ACTIVE`: Confirmation that debug mode was entered

---

## 6.5. Menu Behavior

### SMART Mode (Controller-Managed)

When `CMD MENU_MODE SMART`:

**Left Button:**
- Visual: Slow breathing LED in idle
- On press: Decrements menu_index (with wrap to last item), emits `EVT MENU_MOVE <index>`

**Right Button:**
- Visual: Slow breathing LED in idle
- On press: Increments menu_index (with wrap to 0), emits `EVT MENU_MOVE <index>`

**Center Button:**
- Visual: Breathing "hit me" pattern
- On press: Emits `EVT MENU_SELECT <index>`

The host receives events and can update UI or change modes accordingly. The controller handles all menu index logic.

### DUMB Mode (Host-Managed)

When `CMD MENU_MODE DUMB`:

- Host sets `CMD MENU_INDEX <i>` to control which item is highlighted
- Button presses still generate raw `EVT BUTTON` events
- No `EVT MENU_MOVE` or `EVT MENU_SELECT` events are generated
- Controller only updates visual indicators based on host-provided index

---

## 6.6. Game Flow Signals

### Ball Ready Sequence

1. Host sets `CMD MODE GAME`
2. Host updates `CMD SCORE` / `CMD BALLS` as needed
3. **Host sets `CMD STATE BALL_READY 1` when ball is loaded**
4. Controller automatically:
   - Sets substate to SUB_BALL_READY
   - Activates BALL_LAUNCH NeoPixel effect
   - Activates CENTER_HIT_PULSE on center button
5. Player presses center button
6. Controller:
   - Emits `EVT BUTTON CENTER DOWN` (raw button event)
   - Sets ball_ready = false, substate = SUB_BALL_IN_PLAY
   - Reverts to in-play base profile
7. Host receives `EVT BUTTON CENTER DOWN` and knows ball_ready was true, so transitions game logic to ball-in-play

This approach allows the controller to provide immediate tactile feedback while the host manages game state transitions based on raw button events.

---

## 6.7. Complete Game Flow Examples

This section provides complete command sequences for common game flow scenarios, showing the exact order of CMD functions to send for typical pinball game states.

### Example 1: Startup Sequence
```
# Controller boots and sends:
EVT READY

# Host initializes to attract mode:
CMD MODE ATTRACT
CMD BRIGHTNESS 200
CMD SCORE 0
CMD BALLS 3
```

**Result:** Controller enters attract mode with rainbow chase on NeoPixels and ready/steady glow on buttons.

---

### Example 2: Menu Navigation (SMART Mode)
```
# Enter menu from attract mode:
CMD MODE MENU
CMD MENU_MODE SMART
CMD MENU_SIZE 4
CMD MENU_INDEX 0

# Controller now shows MENU_NAVIGATION button effect
# Player presses RIGHT button:
EVT BUTTON RIGHT DOWN      # Raw button event
EVT BUTTON RIGHT UP
EVT MENU_MOVE 1           # Menu navigation event (index now 1)

# Player presses RIGHT again:
EVT BUTTON RIGHT DOWN
EVT BUTTON RIGHT UP
EVT MENU_MOVE 2           # Index now 2

# Player presses CENTER to select:
EVT BUTTON CENTER DOWN
EVT BUTTON CENTER UP
EVT MENU_SELECT 2         # Player selected item 2

# Host processes selection and starts game:
CMD MODE GAME
CMD SCORE 0
CMD BALLS 3
```

---

### Example 3: Ball Launch Sequence (Full Game Start)
```
# Start new game:
CMD MODE GAME
CMD SCORE 0
CMD BALLS 3

# Ball is loaded and ready:
CMD STATE BALL_READY 1

# Controller automatically:
# - Activates BALL_LAUNCH NeoPixel effect
# - Activates CENTER_HIT_PULSE button effect
# - Player sees pulsing center button

# Player presses center to launch:
EVT BUTTON CENTER DOWN    # Raw button event

# Controller automatically clears ball_ready flag
# Host processes the launch and ball is now in play

# During gameplay, flippers are used:
EVT BUTTON LEFT DOWN      # Left flipper activated
EVT BUTTON LEFT UP        # Left flipper released
EVT BUTTON RIGHT DOWN     # Right flipper activated
EVT BUTTON RIGHT UP       # Right flipper released
```

---

### Example 4: Skill Shot Sequence
```
# Ball is ready, activate skill shot window:
CMD STATE BALL_READY 1
CMD STATE SKILL_SHOT 1

# Controller shows skill shot visuals
# Player launches ball:
EVT BUTTON CENTER DOWN

# Ball in play, skill shot window active for 3 seconds
# If player makes skill shot within window:
CMD STATE SKILL_SHOT 0    # Clear skill shot flag
CMD EVENT JACKPOT         # Celebrate with jackpot effect

# Update score:
CMD SCORE 50000
```

---

### Example 5: Ball Save Event
```
# Ball is in play and drains, but ball save is active:
CMD EVENT BALL_SAVED

# Controller plays:
# - RED_STROBE_5X on NeoPixels (1.5s)
# - BALL_SAVED animation on buttons (1.5s)

# Ball is returned to play:
CMD STATE BALL_READY 1

# Player launches again:
EVT BUTTON CENTER DOWN
```

---

### Example 6: Multiball Activation
```
# Player qualifies for multiball:
CMD EVENT MULTIBALL_START

# Controller automatically:
# - Sets multiball_active flag
# - Plays PINK_PULSE NeoPixel effect (2.0s)
# - Plays POWERUP_ALERT button effect (2.0s)

# Multiple balls in play...
CMD SCORE 75000
CMD SCORE 85000
CMD SCORE 100000

# Multiball ends (back to single ball):
CMD EVENT MULTIBALL_END

# Controller reverts to normal gameplay visuals
```

---

### Example 7: Extra Ball Award
```
# Player earns extra ball:
CMD EVENT EXTRA_BALL

# Controller plays:
# - PINK_PULSE on NeoPixels (2.0s)
# - EXTRA_BALL_AWARD on buttons (three pulses + fade, 2.0s)

# Update ball count:
CMD BALLS 4
```

---

### Example 8: Ball Drain Sequence
```
# Ball drains (no ball save):
CMD MODE BALL_LOST

# Controller shows:
# - RED_STROBE_5X on NeoPixels
# - READY_STEADY_GLOW on buttons

# Update ball count:
CMD BALLS 2

# Wait for animation to complete (~1.5s)
# Then load next ball:
CMD MODE GAME
CMD STATE BALL_READY 1

# Player launches next ball:
EVT BUTTON CENTER DOWN
```

---

### Example 9: Game Over Sequence
```
# Final ball drains:
CMD MODE BALL_LOST
CMD BALLS 0

# Brief delay for drama (~2s)
# Then transition to high score mode:
CMD MODE HIGH_SCORE
CMD TEXT 8 2 GAME OVER
CMD DISPLAY_ANIM MAIN_MENU

# Controller shows:
# - PINK_PULSE on NeoPixels
# - READY_STEADY_GLOW on buttons

# Check if high score was achieved...
# If yes, handle high score entry (implementation specific)
# If no, return to attract after delay:
CMD MODE ATTRACT
CMD SCORE 0
CMD BALLS 3
```

---

### Example 10: Jackpot Event
```
# During gameplay, player hits jackpot target:
CMD EVENT JACKPOT

# Controller plays:
# - RAINBOW_WAVE on NeoPixels (2.5s)
# - POWERUP_ALERT on buttons (chaotic strobe, 2.5s)

# Update score with jackpot value:
CMD SCORE 150000
```

---

### Example 11: Menu Navigation (DUMB Mode)
```
# Enter menu with host-controlled navigation:
CMD MODE MENU
CMD MENU_MODE DUMB
CMD MENU_SIZE 5
CMD MENU_INDEX 0

# Host updates visual indicator based on its own navigation:
CMD MENU_INDEX 1    # Highlight item 1
CMD MENU_INDEX 2    # Highlight item 2

# Player presses buttons, host receives raw events only:
EVT BUTTON RIGHT DOWN
EVT BUTTON RIGHT UP

# Host manages its own menu logic and updates index:
CMD MENU_INDEX 3

# Player selects:
EVT BUTTON CENTER DOWN
EVT BUTTON CENTER UP

# Host processes selection (no EVT MENU_SELECT in DUMB mode)
CMD MODE GAME
```

---

### Example 12: Testing/Debug Override Sequence
```
# Enter debug mode:
CMD DEBUG

# Controller sends:
EVT DEBUG ACTIVE

# Override effects for testing:
CMD EFFECT_OVERRIDE WATER
CMD BUTTON_EFFECT_OVERRIDE SKILL_SHOT_BUILDUP

# Effects are now locked to these patterns
# Mode changes won't affect them

# Test for a while...

# Clear overrides:
CMD EFFECT_CLEAR
CMD BUTTON_EFFECT_CLEAR

# Exit debug mode by sending any other command:
CMD MODE ATTRACT
```

---

### Example 13: Complete 3-Ball Game Flow
```
# 1. Attract Mode
CMD MODE ATTRACT
CMD SCORE 0
CMD BALLS 3

# Player presses button to start...
EVT BUTTON CENTER DOWN

# 2. Start Game - Ball 1
CMD MODE GAME
CMD SCORE 0
CMD BALLS 3
CMD STATE BALL_READY 1

EVT BUTTON CENTER DOWN    # Launch ball 1
# ... gameplay ...
CMD SCORE 25000
CMD SCORE 42000

# Ball 1 drains
CMD MODE BALL_LOST
CMD BALLS 2
# ... delay ~1.5s ...

# 3. Ball 2
CMD MODE GAME
CMD STATE BALL_READY 1

EVT BUTTON CENTER DOWN    # Launch ball 2
# ... gameplay ...
CMD EVENT JACKPOT         # Big score!
CMD SCORE 85000

# Ball 2 drains
CMD MODE BALL_LOST
CMD BALLS 1
# ... delay ~1.5s ...

# 4. Ball 3 (final ball)
CMD MODE GAME
CMD STATE BALL_READY 1

EVT BUTTON CENTER DOWN    # Launch ball 3
# ... gameplay ...
CMD EVENT MULTIBALL_START # Multiball on final ball!
CMD SCORE 120000
CMD EVENT MULTIBALL_END

# Final ball drains
CMD MODE BALL_LOST
CMD BALLS 0
# ... delay ~2s ...

# 5. Game Over
CMD MODE HIGH_SCORE
CMD TEXT 6 2 FINAL: 120000
# ... check high score, handle entry if needed ...

# 6. Return to Attract
CMD MODE ATTRACT
CMD SCORE 0
CMD BALLS 3
```

---

# 7. Button Input & LED Effects (Full Consolidated Reference)

### Button Input Behavior
- Polled every ~10 ms
- Debounced
- Generates: DOWN, UP, HELD (>500 ms)
- All events sent to host via `EVT BUTTON`

---

## LED Effect Modes

### 1. OFF
All LEDs off.

### 2. READY_STEADY_GLOW
Idle/attract/menu look.  
- Left & Right: slow offset breathing  
- Center: steady glow ~200 brightness  

### 3. FLIPPER_FEEDBACK
Short “pop” flash when flipper pressed.  
Auto‑returns to READY_STEADY_GLOW.

### 4. CENTER_HIT_PULSE
Double‑strobe repeating; indicates ball ready to launch.

### 5. SKILL_SHOT_BUILDUP
All buttons ramp 0→255 over 2 s, snap to 0; loops.

### 6. BALL_SAVED
Alternating left/right flashes with decaying tempo; 1‑shot.

### 7. POWERUP_ALERT
Chaotic random strobe for jackpots/multiball; 1.5 s.

### 8. EXTRA_BALL_AWARD
Three ~200 ms pulses → 1 s fade; 1‑shot.

### 9. GAME_OVER_FADE
Very slow breathing (2.5 s cycle), continuous.

### 10. MENU_NAVIGATION
Selected button = 255  
Others = ~80  
Controlled via `buttons_set_menu_selection()`.

---

# 8. Matrix Display System

- Four 8×8 HT16K33 matrices → **32×8 logical surface**
- Score uses 5×5 digit font, right‑aligned
- Balls displayed as 2×2 blocks on rows 6–7
- Non‑blocking rendering
- Addresses: 0x70–0x73

---

# 9. NeoPixel Lighting System

### Global Characteristics
- 48 LEDs
- Physical board metadata: position + reversed orientation
- All effects update at 60 FPS; no delays
- Brightness scaling via `CMD BRIGHTNESS`

### Effects
1. **RAINBOW_BREATHE** – slow rainbow + breathing  
2. **RAINBOW_WAVE** – full‑cabinet rainbow sweep  
3. **CAMERA_FLASH** – board 3 flashes white  
4. **RED_STROBE_5X** – 5 fast red flashes  
5. **WATER** – aqua flow animation  
6. **ATTRACT** – idle rainbow chase  
7. **PINK_PULSE** – strong pink pulse  
8. **BALL_LAUNCH** – yellow ripple (front→back)

---

# 10. Debug Mode & Diagnostics (Consolidated)

### Entering Debug Mode
```
CMD DEBUG
```
Firmware replies:
```
EVT DEBUG ACTIVE
```

### Exiting Debug Mode
Any command exits.

### Debug Mode Behavior
- **NeoPixels:** board‑ID chase → rainbow → reverse mapping check  
- **Buttons:** left slow pulse / center steady / right fast pulse  
- **Matrices:** font test, blocks, scrolling patterns  
- **Serial:** verbose logs for I²C communication  

### Diagnostic Logging
During startup:
- I²C init  
- Device detection & configuration  
- Error logs: missing ACK, partial writes  

During runtime:
- Button raw GPIO snapshots  
- Button events  
- Display update failures  
- NeoPixel PIO errors (if any)

### Debug Self-Test Example Output
```
Testing Seesaw (0x3A)... ✓ OK
Testing HT16K33 0x70... ✓ OK
Testing HT16K33 0x71... ✓ OK
Testing HT16K33 0x72... ✓ OK
Testing HT16K33 0x73... ✓ OK
```

---

# 11. Troubleshooting Guide

### Buttons Not Responding
- Check Seesaw at 0x3A  
- Verify SDA/SCL wiring  
- Look for “read FAILED” logs  

### Matrix Not Updating
- Confirm addresses 0x70–0x73  
- Look for `NO ACK` in logs  
- Inspect STEMMA QT cable  

### NeoPixels Flicker / Dead Board
- Use 5V 3A supply  
- Add 330Ω data resistor  
- Ensure common ground  
- Check reversed boards (4 & 5)

### Serial Not Working
- Use real USB data cable  
- Use `screen /dev/ttyACM0` or COM port on Windows  

---

# 12. Implementation Summary

Total firmware size: **~1,100 lines of C + 47 lines of PIO ASM**.

### Modules
- `controller_state.c` – central state machine + priority handling
- `neopixel.c` – WS2812 driver + effects  
- `buttons.c` – Seesaw driver + LED effects  
- `display.c` – HT16K33 driver + rendering  
- `protocol.c` – ASCII protocol  
- `debug_mode.c` – diagnostics & self‑test  
- `main.c` – main loop and orchestrator  

---

# 13. Project Structure

```
firmware/
├── include/
│   ├── hardware_config.h
│   ├── controller_state.h
│   ├── protocol.h
│   ├── neopixel.h
│   ├── buttons.h
│   ├── display.h
│   ├── debug_mode.h
│   └── types.h
└── src/
    ├── main.c
    ├── controller_state.c
    ├── protocol.c
    ├── neopixel.c
    ├── buttons.c
    ├── display.c
    ├── debug_mode.c
    └── ws2812.pio
```

---

# 14. Customization Guide

### Add New NeoPixel Effect
1. Add enum entry  
2. Implement effect in `neopixel.c`  
3. Register in effect dispatcher  

### Add New Button Effect
1. Extend enum in `buttons.h`  
2. Implement pattern logic in `buttons.c`  

### Modify Display Layout
- Edit `display_draw_score()` and `display_draw_balls()`  

---

# 15. Appendix: Recommended Game‑State Mapping

| Game State | NeoPixel Effect | Button Effect | Notes |
|-----------|-----------------|---------------|-------|
| Attract   | ATTRACT | READY_STEADY_GLOW | Idle |
| Menu      | ATTRACT | MENU_NAVIGATION | Selection mode |
| Ball Ready| BALL_LAUNCH | CENTER_HIT_PULSE | Launch indicator |
| Skill Shot| WATER | SKILL_SHOT_BUILDUP | Pre‑launch tension |
| Gameplay  | None | READY_STEADY_GLOW | Stable |
| Ball Save | RED_STROBE_5X | BALL_SAVED | Urgency |
| Multiball | PINK_PULSE | POWERUP_ALERT | Chaotic |
| Extra Ball| PINK_PULSE | EXTRA_BALL_AWARD | Celebration |
| Game Over | None | GAME_OVER_FADE | Cooldown |

---

End of Version A.
