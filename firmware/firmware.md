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

## Architecture: Pi = Brain, Controller = Renderer + IO

The **Raspberry Pi** is the sole owner of all game state (modes, scoring, ball tracking, multiball, skill shots, menu navigation, etc.). The **KB2040 controller** acts as a cabinet renderer and I/O device:

- **Renders** text, scores, and ball counts on the matrix display
- **Executes** button LED effects requested by the Pi
- **Executes** NeoPixel lighting effects requested by the Pi
- **Reports** raw button events (DOWN/UP/HELD) back to the Pi

The controller does **not** manage gameplay modes, game-state flags, or infer higher-level game meaning from button presses. All behavior is driven by explicit commands from the Pi, executed using a local effects library.

## Recent Updates
- **PR #37**: Fixed button LED animation timing and synchronization issues. Button effects now properly respect priority system and transition smoothly between states.

---

# 2. Controller Operation Model

The controller operates in a single **RUN** mode at all times. All visual behavior is driven by **commands from the Pi** combined with a **local effects library**.

## 2.1. Effects Library

The controller maintains a library of reusable visual effects that the Pi can trigger on demand:

### Button LED Effects
- `OFF`, `READY_STEADY_GLOW`, `FLIPPER_FEEDBACK`, `CENTER_HIT_PULSE`
- `SKILL_SHOT_BUILDUP`, `BALL_SAVED`, `POWERUP_ALERT`, `EXTRA_BALL_AWARD`
- `GAME_OVER_FADE`, `MENU_NAVIGATION`

### NeoPixel Effects
- `RAINBOW_BREATHE`, `RAINBOW_WAVE`, `CAMERA_FLASH`, `RED_STROBE_5X`
- `WATER`, `ATTRACT`, `PINK_PULSE`, `BALL_LAUNCH`, `NONE`

The **Pi chooses** which effect to run and when to change or stop it. Effects run locally on the controller once requested.

## 2.2. Effect Priority (Implementation Detail)

For resolving conflicting effect requests, the controller uses a simple priority system:

| Priority | Description | Use Cases |
|----------|-------------|-----------|
| **BASE** | Normal effects | Standard gameplay visuals |
| **EVENT** | Temporary effects | Short-duration events (1-3s), auto-revert to BASE |
| **CRITICAL** | Override effects | Manual/debug commands, persist until cleared |

**Important:** This priority system is purely an implementation detail for handling simultaneous requests. It does **not** imply hidden game states or autonomous transitions. The Pi still decides what effects to send and when.

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

## 6.1. Display Commands (Pi → KB2040)

Commands for controlling the LED matrix display content.

### Score & Display
```
CMD DISPLAY SCORE <number>
CMD DISPLAY BALLS <number>
CMD DISPLAY TEXT <string>
CMD DISPLAY CLEAR
```

- `DISPLAY SCORE <number>`: Updates the LED matrix with the current score
- `DISPLAY BALLS <number>`: Updates the ball count display
- `DISPLAY TEXT <string>`: Displays custom text on the 32×8 matrix
- `DISPLAY CLEAR`: Clears the entire display

**Examples:**
```
CMD DISPLAY SCORE 42500
CMD DISPLAY BALLS 2
CMD DISPLAY TEXT GAME OVER
CMD DISPLAY CLEAR
```

### Brightness
```
CMD NEO BRIGHTNESS <0-255>
```

Sets global NeoPixel brightness (0-255). Does not affect button LEDs.

**Example:**
```
CMD NEO BRIGHTNESS 200
```

### System Commands
```
CMD PING
CMD DEBUG
```

- `PING`: Controller responds with `EVT PONG`
- `DEBUG`: Enter debug/diagnostic mode (see Section 10)

---

## 6.2. Effect Commands (Pi → KB2040)

Commands for triggering visual effects on buttons and NeoPixels. The Pi explicitly requests effects based on game state transitions.

### Button Effects
```
CMD BUTTON EFFECT <LEFT|CENTER|RIGHT|ALL> <EFFECT_NAME>
CMD BUTTON EFFECT CLEAR
```

Available button effects:
- `OFF`, `READY_STEADY_GLOW`, `FLIPPER_FEEDBACK`, `CENTER_HIT_PULSE`
- `SKILL_SHOT_BUILDUP`, `BALL_SAVED`, `POWERUP_ALERT`, `EXTRA_BALL_AWARD`
- `GAME_OVER_FADE`, `MENU_NAVIGATION`

**Examples:**
```
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE    # Center button pulses for ball launch
CMD BUTTON EFFECT ALL READY_STEADY_GLOW      # All buttons to idle glow
CMD BUTTON EFFECT CLEAR                       # Stop all button effects
```

### NeoPixel Effects
```
CMD NEO EFFECT <EFFECT_NAME>
CMD NEO EFFECT CLEAR
```

Available NeoPixel effects:
- `RAINBOW_BREATHE`, `RAINBOW_WAVE`, `CAMERA_FLASH`, `RED_STROBE_5X`
- `WATER`, `ATTRACT`, `PINK_PULSE`, `BALL_LAUNCH`, `NONE`

**Examples:**
```
CMD NEO EFFECT ATTRACT           # Idle rainbow chase
CMD NEO EFFECT BALL_LAUNCH       # Yellow ripple for ball launch
CMD NEO EFFECT PINK_PULSE        # Pink pulse for celebration
CMD NEO EFFECT CLEAR             # Stop current effect
```

---

## 6.3. Temporary Event Effects (Pi → KB2040)

For short-lived game events (jackpot, ball save, extra ball, etc.), the Pi can use these convenience commands that automatically revert after a preset duration. These are simply shortcuts that combine an effect command with an automatic timer.

```
CMD EVENT BALL_SAVED        # RED_STROBE_5X (1.5s) + BALL_SAVED buttons
CMD EVENT EXTRA_BALL        # PINK_PULSE (2.0s) + EXTRA_BALL_AWARD buttons
CMD EVENT JACKPOT           # RAINBOW_WAVE (2.5s) + POWERUP_ALERT buttons
CMD EVENT MULTIBALL_START   # PINK_PULSE (2.0s) + POWERUP_ALERT buttons
```

These event commands are **optional conveniences**. The Pi can achieve the same result by sending explicit effect commands and managing timing itself.

**Example equivalents:**
```
# Using event command:
CMD EVENT JACKPOT

# Equivalent using explicit commands:
CMD NEO EFFECT RAINBOW_WAVE
CMD BUTTON EFFECT ALL POWERUP_ALERT
# (Pi waits 2.5s, then sends new effects)
```

---

## 6.4. Events (KB2040 → Pi)

The controller sends **only raw button events** back to the Pi. The Pi interprets these events based on its own game state.

### Button Events
```
EVT BUTTON <LEFT|CENTER|RIGHT> <DOWN|UP|HELD>
```

Raw button state changes:
- `DOWN`: Button pressed
- `UP`: Button released
- `HELD`: Button held for >500ms

**Examples:**
```
EVT BUTTON LEFT DOWN
EVT BUTTON LEFT UP
EVT BUTTON CENTER HELD
EVT BUTTON RIGHT DOWN
```

The controller does **not** interpret these as "menu move," "menu select," "ball launch," etc. The Pi decides what each button press means based on the current game state.

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

## 6.5. Complete Game Flow Examples

This section provides complete command sequences for common game flow scenarios. The Pi sends explicit effect and display commands based on its own game state; the controller simply executes them.

### Example 1: Startup Sequence (Attract Mode)
```
# Controller boots and sends:
EVT READY

# Pi initializes to attract mode:
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
CMD NEO BRIGHTNESS 200
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3
```

**Result:** Controller displays attract visuals—rainbow chase on NeoPixels and idle glow on buttons.

---

### Example 2: Menu Navigation
```
# Pi enters menu mode (Pi manages menu state):
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL MENU_NAVIGATION
CMD DISPLAY TEXT MAIN MENU

# Pi tracks menu_index internally (e.g., starts at 0)
# Player presses RIGHT button:
EVT BUTTON RIGHT DOWN
EVT BUTTON RIGHT UP

# Pi increments menu_index to 1, updates display:
CMD DISPLAY TEXT START GAME

# Player presses RIGHT again:
EVT BUTTON RIGHT DOWN
EVT BUTTON RIGHT UP

# Pi increments menu_index to 2, updates display:
CMD DISPLAY TEXT OPTIONS

# Player presses CENTER to select:
EVT BUTTON CENTER DOWN
EVT BUTTON CENTER UP

# Pi processes selection based on menu_index and starts game:
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3
```

---

### Example 3: Ball Launch Sequence (Full Game Start)
```
# Pi starts new game and sets up ball-ready visuals:
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE
CMD BUTTON EFFECT LEFT READY_STEADY_GLOW
CMD BUTTON EFFECT RIGHT READY_STEADY_GLOW

# Pi is now in "ball_ready" state (tracked on Pi side)
# Player sees pulsing center button indicating ready to launch

# Player presses center to launch:
EVT BUTTON CENTER DOWN

# Pi receives button event, knows it was in ball_ready state, transitions to ball-in-play:
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# During gameplay, flippers are used:
EVT BUTTON LEFT DOWN      # Left flipper activated
EVT BUTTON LEFT UP        # Left flipper released
EVT BUTTON RIGHT DOWN     # Right flipper activated
EVT BUTTON RIGHT UP       # Right flipper released
```

---

### Example 4: Skill Shot Sequence
```
# Pi activates skill shot window with special visuals:
CMD NEO EFFECT WATER
CMD BUTTON EFFECT ALL SKILL_SHOT_BUILDUP
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE

# Player launches ball:
EVT BUTTON CENTER DOWN

# Pi transitions to ball-in-play with skill shot window active (tracked on Pi):
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# Pi waits 3 seconds for skill shot target...
# If player makes skill shot within window:
CMD EVENT JACKPOT         # Celebrate with jackpot effect

# Update score:
CMD DISPLAY SCORE 50000
```

---

### Example 5: Ball Save Event
```
# Ball drains, but Pi's ball save is active:
CMD EVENT BALL_SAVED

# Controller plays automatic effect:
# - RED_STROBE_5X on NeoPixels (1.5s)
# - BALL_SAVED animation on buttons (1.5s)

# After effect completes, Pi sets up ball ready again:
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE
CMD BUTTON EFFECT LEFT READY_STEADY_GLOW
CMD BUTTON EFFECT RIGHT READY_STEADY_GLOW

# Player launches again:
EVT BUTTON CENTER DOWN
```

---

### Example 6: Multiball Activation
```
# Pi detects multiball qualification and triggers celebration:
CMD EVENT MULTIBALL_START

# Controller plays automatic effect:
# - PINK_PULSE NeoPixel effect (2.0s)
# - POWERUP_ALERT button effect (2.0s)

# Multiple balls in play (Pi tracks multiball state)...
CMD DISPLAY SCORE 75000
CMD DISPLAY SCORE 85000
CMD DISPLAY SCORE 100000

# Multiball ends (Pi detects back to single ball):
# Pi returns to normal gameplay visuals:
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
```

---

### Example 7: Extra Ball Award
```
# Pi detects extra ball earned:
CMD EVENT EXTRA_BALL

# Controller plays automatic effect:
# - PINK_PULSE on NeoPixels (2.0s)
# - EXTRA_BALL_AWARD on buttons (three pulses + fade, 2.0s)

# Update ball count:
CMD DISPLAY BALLS 4
```

---

### Example 8: Ball Drain Sequence
```
# Ball drains (Pi detects, no ball save):
CMD NEO EFFECT RED_STROBE_5X
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# Update ball count:
CMD DISPLAY BALLS 2

# Wait for effect to complete (~1.5s)
# Then Pi loads next ball with ball-ready visuals:
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE

# Player launches next ball:
EVT BUTTON CENTER DOWN

# Pi transitions to ball-in-play:
CMD NEO EFFECT NONE
```

---

### Example 9: Game Over Sequence
```
# Final ball drains (Pi detects):
CMD NEO EFFECT RED_STROBE_5X
CMD DISPLAY BALLS 0

# Brief delay for drama (~2s)
# Then Pi transitions to game over display:
CMD NEO EFFECT PINK_PULSE
CMD BUTTON EFFECT ALL GAME_OVER_FADE
CMD DISPLAY TEXT GAME OVER

# Pi checks if high score was achieved...
# If yes, handle high score entry (implementation specific)
# If no, return to attract after delay:
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3
```

---

### Example 10: Jackpot Event
```
# During gameplay, Pi detects jackpot target hit:
CMD EVENT JACKPOT

# Controller plays automatic effect:
# - RAINBOW_WAVE on NeoPixels (2.5s)
# - POWERUP_ALERT on buttons (chaotic strobe, 2.5s)

# Update score with jackpot value:
CMD DISPLAY SCORE 150000
```

---

### Example 11: Explicit Effect Testing
```
# Pi wants to test specific effect combinations:
CMD NEO EFFECT WATER
CMD BUTTON EFFECT LEFT SKILL_SHOT_BUILDUP
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE
CMD BUTTON EFFECT RIGHT READY_STEADY_GLOW

# Effects run as requested
# Pi can change them at any time:
CMD NEO EFFECT RAINBOW_WAVE
CMD BUTTON EFFECT ALL POWERUP_ALERT

# Clear everything:
CMD NEO EFFECT NONE
CMD BUTTON EFFECT CLEAR
```

---

### Example 12: Debug Mode
```
# Enter debug mode for hardware diagnostics:
CMD DEBUG

# Controller sends:
EVT DEBUG ACTIVE

# Controller runs hardware self-test patterns
# (See Section 10 for debug mode details)

# Exit debug mode and return to normal operation:
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
```

---

### Example 13: Complete 3-Ball Game Flow
```
# 1. Attract Mode
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3

# Player presses button to start...
EVT BUTTON CENTER DOWN

# 2. Start Game - Ball 1 Ready
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3

EVT BUTTON CENTER DOWN    # Launch ball 1

# Pi transitions to ball-in-play:
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# ... gameplay ...
CMD DISPLAY SCORE 25000
CMD DISPLAY SCORE 42000

# Ball 1 drains
CMD NEO EFFECT RED_STROBE_5X
CMD DISPLAY BALLS 2
# ... delay ~1.5s ...

# 3. Ball 2 Ready
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE

EVT BUTTON CENTER DOWN    # Launch ball 2
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# ... gameplay ...
CMD EVENT JACKPOT         # Big score!
CMD DISPLAY SCORE 85000

# Ball 2 drains
CMD NEO EFFECT RED_STROBE_5X
CMD DISPLAY BALLS 1
# ... delay ~1.5s ...

# 4. Ball 3 (final ball) Ready
CMD NEO EFFECT BALL_LAUNCH
CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE

EVT BUTTON CENTER DOWN    # Launch ball 3
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# ... gameplay ...
CMD EVENT MULTIBALL_START # Multiball on final ball!
CMD DISPLAY SCORE 120000
# ... multiball ends ...
CMD NEO EFFECT NONE
CMD BUTTON EFFECT ALL READY_STEADY_GLOW

# Final ball drains
CMD NEO EFFECT RED_STROBE_5X
CMD DISPLAY BALLS 0
# ... delay ~2s ...

# 5. Game Over
CMD NEO EFFECT PINK_PULSE
CMD BUTTON EFFECT ALL GAME_OVER_FADE
CMD DISPLAY TEXT FINAL: 120000
# ... check high score, handle entry if needed ...

# 6. Return to Attract
CMD NEO EFFECT ATTRACT
CMD BUTTON EFFECT ALL READY_STEADY_GLOW
CMD DISPLAY SCORE 0
CMD DISPLAY BALLS 3
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

This table shows how the **Pi** can map its game states to controller effects. The controller does not know what "Attract," "Ball Ready," "Multiball," etc. mean—it only knows which effect commands it receives and executes.

| Pi Game State | Recommended Commands | Notes |
|---------------|---------------------|-------|
| **Attract** | `CMD NEO EFFECT ATTRACT`<br>`CMD BUTTON EFFECT ALL READY_STEADY_GLOW` | Idle/demo mode |
| **Menu** | `CMD NEO EFFECT ATTRACT`<br>`CMD BUTTON EFFECT ALL MENU_NAVIGATION` | Menu navigation visuals |
| **Ball Ready** | `CMD NEO EFFECT BALL_LAUNCH`<br>`CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE`<br>`CMD BUTTON EFFECT LEFT READY_STEADY_GLOW`<br>`CMD BUTTON EFFECT RIGHT READY_STEADY_GLOW` | Launch indicator |
| **Skill Shot** | `CMD NEO EFFECT WATER`<br>`CMD BUTTON EFFECT ALL SKILL_SHOT_BUILDUP` | Pre‑launch tension |
| **Ball In Play** | `CMD NEO EFFECT NONE`<br>`CMD BUTTON EFFECT ALL READY_STEADY_GLOW` | Normal gameplay |
| **Ball Save** | `CMD EVENT BALL_SAVED` | Urgency (auto-effect) |
| **Multiball** | `CMD EVENT MULTIBALL_START` | Celebration (auto-effect) |
| **Extra Ball** | `CMD EVENT EXTRA_BALL` | Award celebration (auto-effect) |
| **Jackpot** | `CMD EVENT JACKPOT` | Big score celebration (auto-effect) |
| **Ball Drain** | `CMD NEO EFFECT RED_STROBE_5X`<br>`CMD BUTTON EFFECT ALL READY_STEADY_GLOW` | Drain sequence |
| **Game Over** | `CMD NEO EFFECT PINK_PULSE`<br>`CMD BUTTON EFFECT ALL GAME_OVER_FADE` | End of game |

**Key Point:** These mappings are **implemented on the Pi** by sending the corresponding effect commands. The controller does not autonomously transition between states or infer game flow from button presses.

---

End of Version A.
