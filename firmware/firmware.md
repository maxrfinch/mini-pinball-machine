# Arcade Pinball Controller Firmware – Full Consolidated Documentation (Version A)

## Table of Contents
1. System Overview
2. Hardware Architecture
3. Wiring & Setup
4. Build & Flash Instructions
5. Host Communication Protocol
6. Button Input & LED Effects (Full Reference)
7. Matrix Display System
8. NeoPixel Lighting System
9. Debug Mode & Diagnostics (Consolidated)
10. Troubleshooting Guide
11. Implementation Summary
12. Project Structure
13. Customization Guide
14. Appendix: Recommended Game‑State Mapping

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

---

# 2. Hardware Architecture

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

# 3. Wiring & Setup

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

# 4. Build & Flash Instructions

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

# 5. Host Communication Protocol

ASCII messages over USB CDC, newline‑terminated.

### Commands (Pi → KB2040)
```
CMD MODE <ATTRACT|MENU|GAME|BALL_LOST|HIGH_SCORE>
CMD SCORE <number>
CMD BALLS <number>
CMD EFFECT <pattern>
CMD BUTTON_EFFECT <pattern>
CMD BRIGHTNESS <0-255>
CMD PING
CMD DEBUG
```

### Events (KB2040 → Pi)
```
EVT READY
EVT PONG
EVT DEBUG ACTIVE
EVT BUTTON <LEFT|CENTER|RIGHT> <DOWN|UP|HELD>
```

---

# 6. Button Input & LED Effects (Full Consolidated Reference)

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

# 7. Matrix Display System

- Four 8×8 HT16K33 matrices → **32×8 logical surface**
- Score uses 5×5 digit font, right‑aligned
- Balls displayed as 2×2 blocks on rows 6–7
- Non‑blocking rendering
- Addresses: 0x70–0x73

---

# 8. NeoPixel Lighting System

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

# 9. Debug Mode & Diagnostics (Consolidated)

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

# 10. Troubleshooting Guide

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

# 11. Implementation Summary

Total firmware size: **~1,100 lines of C + 47 lines of PIO ASM**.

### Modules
- `neopixel.c` – WS2812 driver + effects  
- `buttons.c` – Seesaw driver + LED effects  
- `display.c` – HT16K33 driver + rendering  
- `protocol.c` – ASCII protocol  
- `debug_mode.c` – diagnostics & self‑test  
- `main.c` – main loop and orchestrator  

---

# 12. Project Structure

```
firmware/
├── include/
│   ├── hardware_config.h
│   ├── protocol.h
│   ├── neopixel.h
│   ├── buttons.h
│   ├── display.h
│   ├── debug_mode.h
│   └── types.h
└── src/
    ├── main.c
    ├── protocol.c
    ├── neopixel.c
    ├── buttons.c
    ├── display.c
    ├── debug_mode.c
    └── ws2812.pio
```

---

# 13. Customization Guide

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

# 14. Appendix: Recommended Game‑State Mapping

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
