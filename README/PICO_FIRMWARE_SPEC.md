# Pico Firmware Specification (Phase 1)

This document defines the hardware interface, serial protocol, and required behaviors for the Raspberry Pi Pico, which replaces the Arduino Uno used in the original mini‑pinball machine. It includes the full Phase‑1 specification and clearly marked TODO sections for future Phase‑2 and Phase‑3 features.

---

## 1. Hardware Overview

The Pico is responsible for:

1. Reading **button inputs** using the Adafruit LED Arcade Button 1×4 QT I2C Breakout.
2. Driving the **4× 8×8 matrix LED displays** (score, balls, game state).
3. Communicating with a Raspberry Pi 4/5 over USB CDC or UART.
4. Providing a clean foundation for future features:
   - 65 NeoPixel LEDs (64 game LEDs + 1 power LED)
   - LED control for arcade buttons
   - Haptic motors (left/right)

Phase‑1 focuses on input + matrix display + basic serial protocol.

Future features remain TODOs.

---

## 2. Physical Hardware

### 2.1 Raspberry Pi Pico

* Runs firmware, event loop, and command parser.
* Communicates with Pi5 via USB CDC or UART.

### 2.2 Adafruit LED Arcade Button 1×4 QT Board

Used for:
* Button inputs (3 buttons)
* LED driving in future (Phase‑2)

**Button mapping:**

| QT Index | Button | Logical Role  |
| -------- | ------ | ------------- |
| 0        | Left   | Left flipper  |
| 1        | Center | Start/Menu    |
| 2        | Right  | Right flipper |
| 3        | —      | Reserved      |

**I2C Configuration:**
* Address: 0x30
* SDA: GPIO 4
* SCL: GPIO 5
* Baudrate: 100 kHz

### 2.3 4× 8×8 Matrix Displays

Used for score + ball display.

They form a **32×8 grid**:
* Width: 32 columns (0–31)
* Height: 8 rows (0–7)
* Matrix order: left to right

**I2C Configuration:**
* Driver: HT16K33
* Addresses: 0x70, 0x71, 0x72, 0x73
* Same I2C bus as buttons (I2C0)

### 2.4 Future Hardware (Not in Phase‑1)

#### NeoPixels (Phase‑2)
* LEDs 0–63: gameplay
* LED 64: power LED

#### Haptic Motors (Phase‑3)
* Left and right motors

---

## 3. Serial Protocol (Pi → Pico)

ASCII, newline-terminated:

```
COMMAND arg1 arg2 ...\n
```

Unknown commands are ignored safely.

### **Phase‑1 Required Commands**

#### `SCORE <value>`

Update score display.

Examples:
```
SCORE 12345
```

#### `BALLS <count>`

Update ball indicators.

```
BALLS 3
```

#### `STATE <id>`

Game state ID:

| ID | Meaning   |
| -- | --------- |
| 0  | MENU      |
| 1  | GAME      |
| 2  | GAME_OVER |

```
STATE 1
```

---

## 4. Serial Protocol (Pico → Pi)

Button state is transmitted as a **single byte**, same as the old Arduino system.

### Bit layout:

| Bit | Meaning |
| --- | ------- |
| 2   | Left    |
| 1   | Right   |
| 0   | Center  |

Examples:
* `0x04` — left
* `0x02` — right
* `0x01` — center
* `0x05` — left + center

Send when button state changes or at a periodic polling interval (10ms).

---

## 5. Matrix Display Rendering Specification

The Pico must render score and balls on the 32×8 grid.

### 5.1 Score Rendering

* Score drawn in **rows 0–4**.
* Characters are **max 5 pixels tall**.
* Bottom row(s) not used for score.
* Score is **right‑aligned**:
  - Least significant digit at far right.
  - Digits drawn leftwards.

#### Digit font requirements

* Pico defines a simple 3–5 pixel wide × 5 pixel tall digit set.
* Only numeric characters required.

#### Rendering steps

1. Convert score to string.
2. Clear score area (rows 0–4).
3. Blit digits from right to left.
4. Do **not** touch rows 6–7 (ball display).

### 5.2 Ball Count Rendering

Balls displayed as **2×2 blocks** at the **bottom‑right**.

#### Coordinates

Rows used:
* `6` and `7`

Each ball:
* A 2×2 lit block
* Separated by **2 off columns** (4 columns total per ball)

#### Example mapping

Ball 1 (rightmost):
* Cols `31` and `30`
* Rows `7` and `6`

Ball 2:
* Cols `27` and `26`
* Rows `7` and `6`

Ball 3:
* Cols `23` and `22`
* Rows `7` and `6`

#### Rendering rules

* On `BALLS <count>`:
  - Clear rows 6–7 in ball region
  - Draw N blocks (right to left)

### 5.3 Coexistence of Score + Balls

* Score uses rows 0–4.
* Balls use rows 6–7.
* Firmware **must not** clear the other system's rows.
* A 32×8 framebuffer is used for efficient rendering.

---

## 6. Pico Firmware Architecture

### Directory layout:

```
/firmware
  /src
    main.c              - Main event loop
    protocol.c          - Command parser (SCORE, BALLS, STATE)
    hw_serial.c         - USB CDC communication
    hw_buttons.c        - QT I2C button polling
    hw_display.c        - Matrix display rendering
    # Phase 2 stubs:
    hw_neopixel.c       - NeoPixel control (TODO)
    hw_button_leds.c    - Button LED control (TODO)
    # Phase 3 stubs:
    hw_haptics.c        - Haptic motor control (TODO)
  CMakeLists.txt        - Pico SDK build configuration
  build.sh              - Build script
  README.md             - Documentation
```

### Main loop pseudocode

```c
while (1) {
    poll_buttons();            // QT I2C (every 10ms)
    process_incoming_serial(); // SCORE/BALLS/STATE
    update_display();          // Matrix updates as needed

    // TODO: future expansions
    // update_neopixels();
    // update_button_leds();
    // update_haptics();
}
```

---

## 7. TODO: Future Commands (Do Not Implement Yet)

### NeoPixels (Phase 2)

```
NEO_SET <idx> <r> <g> <b>
NEO_FILL <r> <g> <b>
NEO_MODE <modeId> [params]
NEO_POWER_MODE <modeId>
```

### Button LEDs (Phase 2)

```
BTN_LED <idx> <mode> <params>
```

### Haptics (Phase 3)

```
HAPT <side> <pattern> <strength> <duration>
```

---

## 8. Phase‑1 Implementation Status

### ✅ Required for first working build

* ✅ Implement USB CDC/UART serial (`hw_serial.c`)
* ✅ Implement QT I2C button polling (`hw_buttons.c`)
* ✅ Implement score/ball display system (`hw_display.c`)
* ✅ Implement parser for `SCORE`, `BALLS`, `STATE` (`protocol.c`)
* ✅ Implement main loop (`main.c`)

### ✅ Phase‑2 prep (leave stubs)

* ✅ Initialize NeoPixel subsystem (stub)
* ✅ Stub `NEO_*` commands
* ✅ Stub button LED control

### ✅ Phase‑3 prep

* ✅ Stub haptic control driver
* ✅ Stub `HAPT` command

---

## 9. Building and Flashing

### Prerequisites

1. Pico SDK installed
2. Set `PICO_SDK_PATH` environment variable
3. CMake and build tools

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
make
```

### Flashing

1. Hold BOOTSEL button on Pico
2. Connect USB cable
3. Copy `build/pinball_firmware.uf2` to the Pico's USB drive
4. Pico will reboot automatically

---

## 10. Integration with Raspberry Pi

The Pi-side code (`src/inputManagerPi.c`) has been updated to use the new protocol:

**Old Protocol:**
* `m\n` (menu)
* `r\n` (game)
* `g\n` (game over)
* `s=<score>\n`
* `b=<balls>\n`

**New Protocol:**
* `STATE 0\n` (menu)
* `STATE 1\n` (game)
* `STATE 2\n` (game over)
* `SCORE <value>\n`
* `BALLS <count>\n`

The button reading protocol remains unchanged (single byte with bit flags).

---

## 11. Testing

### Without Hardware

The firmware will compile but cannot be tested without:
* Raspberry Pi Pico
* Adafruit NeoKey 1×4 QT board
* 4× HT16K33 matrix displays

### With Hardware

1. Flash firmware to Pico
2. Connect I2C devices
3. Connect USB to Raspberry Pi
4. Run Pi-side application
5. Verify:
   - Button presses are detected
   - Score displays correctly (right-aligned)
   - Ball count shows correct 2×2 blocks
   - State changes are handled

---

## 12. Known Limitations

* Phase 1 only - NeoPixels, button LEDs, and haptics are stubbed
* Requires proper I2C pull-up resistors
* Display brightness is hardcoded to maximum
* No error handling for I2C communication failures
* Button debouncing relies on polling interval (10ms)

---

## 13. Future Enhancements (Phase 2 & 3)

### Phase 2
* Full NeoPixel support (65 LEDs)
* Animations and effects
* Button LED patterns
* Power LED modes

### Phase 3
* Haptic feedback
* Pattern-based vibrations
* Intensity control
* Synchronized haptics with game events
