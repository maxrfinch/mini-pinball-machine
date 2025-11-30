# Hardware Build Guide


This document provides a clean, high-level, modular structure for assembling all electronics for the Mini Pinball Machine. Each section will later be expanded with detailed wiring notes, diagrams, and configuration steps.

This guide assumes you are comfortable with basic electronics tasks (soldering, wire stripping, crimping, JST connectors). It focuses on system architecture and correct component integration.

---

## Table of Contents

1. **Power System & Raspberry Pi 5**
   - UPS (Geekworm X1206)
   - Battery configuration
   - Power button wiring
   - Pi 5 software configuration (voltage limits, boot settings)
   - Charging port setup
2. **KB2040 Controller Setup**
   - Flashing firmware
   - Connecting I2C, button inputs, and serial communication
   - Mounting considerations
3. **Four Matrix Displays & Button IO Board**
   - Adafruit LED Matrix wiring
   - Arcade button breakout board
   - Power & signal routing
4. **NeoPixel System (Up to 48 LEDs)**
   - KB2040 GPIO assignment
   - Power injection notes
   - Recommended wire gauge & grounding strategy
5. **Audio System**
   - Adafruit I2S 3W Stereo Bonnet setup
   - Speaker wiring
   - TT25 haptic transducer & driver wiring
   - Software configuration
6. **Camera Module**
   - Mounting
   - Pi configuration
   - High-score capture integration notes
7. **Final Integration**
   - Cable management
   - Charging
   - Power button behavior
   - Full system test checklist

---

## 1. Power System & Raspberry Pi 5

This system uses the Geekworm X1206 UPS HAT with four 21700 lithium-ion cells. The UPS mounts directly beneath the Raspberry Pi 5 using pogo-pin contacts, making installation extremely straightforward and eliminating external power bricks.

### 1.1 Install the Raspberry Pi 5 Onto the UPS
1. Place the UPS board on your work surface.
2. Align the Raspberry Pi 5 over the pogo-pin array.
3. Secure the Pi to the standoffs using the supplied screws.

The pogo pins automatically deliver:
- Power input
- Safe shutdown signaling
- Battery monitoring (via I2C)

No jumper wires are required for core power delivery.

### 1.2 Insert the 21700 Batteries

The UPS requires four 21700 cells (e.g., Samsung 5E). Ensure correct polarity as marked on the PCB. All four cells must be installed for proper operation.

Once installed, the UPS LED indicators will display charge status during initial power-up.

### 1.3 Prepare the microSD Card
1. Use Raspberry Pi Imager to flash 64‑bit Raspberry Pi OS (Trixie).
2. A 32 GB card works, though 64 GB is recommended for long-term growth.
3. Insert the card into the Pi and power on via the UPS.
4. Complete the normal first‑boot setup.

### 1.4 Configure EEPROM Power Settings

These settings ensure:
- Maximum allowable current for the UPS
- Full shutdown behavior (no parasitic drain)

Open EEPROM config:
```bash
sudo rpi-eeprom-config -e
```

Add the following lines to the bottom of the file:
```bash
PSU_MAX_CURRENT=5000
POWER_OFF_ON_HALT=1
```

Why this matters
- PSU_MAX_CURRENT=5000 allows the UPS to deliver up to 5A safely.
- POWER_OFF_ON_HALT=1 ensures the Raspberry Pi fully powers down, which forces the UPS to fully shut off as well.
This prevents slow battery drain when the device is off.

### 1.5 Perform an Initial Full Charge

Before proceeding with the rest of the electronics:
- Connect a barrel‑jack power supply to the UPS.
- Allow the system to reach a full charge.

This conditions the cells and ensures accurate battery reporting when you begin testing.

---

## 2. Controller Setup

The controller board (Adafruit KB2040) handles all buttons, four matrix displays, and NeoPixel lighting. Offloading these tasks keeps the Raspberry Pi 5 focused entirely on video, audio, and game logic.

The controller is compact, reliable, and includes a dedicated reset button, making firmware development and troubleshooting straightforward.

### 2.1 Flashing the Controller Firmware

The controller firmware does not need to be compiled manually. A ready-to-flash UF2 file is already included in the repository under:

firmware/build/

You can simply drag‑and‑drop this UF2 file when the controller is in bootloader mode.

Steps to Enter Bootloader Mode (Required First-Time Only)
1. Connect the controller via USB-C.
2. The onboard NeoPixel should cycle colors—this confirms power.
3. Press and hold the BOOT button.
4. While holding BOOT, tap RESET once.
5. Continue holding BOOT until the controller appears as a mounted USB drive.

This initial hardware sequence is required because the controller cannot enter BOOTSEL mode via software until the firmware that supports software-triggered resets is installed.

Flashing
- Drag-and-drop the provided UF2 file from firmware/build/ onto the mounted drive.
- The controller will reboot automatically.

About the Shell Flash Script

After the first manual flash, the installed firmware includes a command that forces the controller into BOOTSEL mode via software. The included shell script uses this feature so you can:
- Reflash the controller without removing it from the cabinet
- Quickly update firmware during development

This convenience is especially important because the controller sits deep inside the case once assembled.

### 2.2 Verifying Controller Operation

Open a terminal on any OS and connect to the controller’s serial port. Once connected, type:
```bash
CMD DEBUG
```

This puts the controller into debug mode.

Expected Behavior in Debug Mode
- The onboard NeoPixel turns orange, confirming proper firmware operation.
- Once matrix displays, buttons, and NeoPixels are wired, debug mode will:
  - Cycle through matrix test patterns
  - Cycle arcade button LED states
  - Cycle all NeoPixels across their full range

Debug mode is one of the fastest ways to confirm wiring and firmware before integrating with the Raspberry Pi.

### 2.3 Summary of Controller Responsibilities

The controller manages:
- All arcade button inputs
- All 7‑segment or LED-matrix displays
- Up to 48 addressable NeoPixels

This hardware separation dramatically reduces CPU load on the Pi 5 and ensures low-latency lighting and button response.

---

## 3. Matrix Displays & Adafruit Arcade Button IO Board

This section covers two tightly related pieces:
- The arcade button I/O board (handles LED and switch wiring for the three main buttons)
- The four I²C LED matrix displays and their address configuration

All of this communicates back to the controller via STEMMA QT (I²C) connectors.

---

### 3.1 Arcade Button I/O Board

The arcade button board simplifies wiring by separating button switches and button LEDs into two neat rows of connectors.

Each arcade button has:
- A pair of terminals for the switch
- A pair of terminals for the LED

On the I/O board you will see two parallel rows of connectors:
- One row is labeled for LEDs
- One row is labeled for switches

Refer to the silkscreen on the board for exact marking.

#### 3.1.1 Connecting Buttons and LEDs
1. Identify the LED terminals on each physical button (these are separate from the gray switch body).
2. Run 2-conductor cables from each LED to the LED row on the I/O board.
3. Run 2-conductor cables from each switch to the switch row on the I/O board.

The game uses three buttons:
- Left
- Center
- Right

The I/O board has four positions, but only three are used.

#### 3.1.2 Button Position Mapping

With the board oriented so that:
- The silkscreen is readable
- The STEMMA QT connectors are on the right side

From left to right, the connected positions are:
1. Left button
2. Center button
3. Right button
4. Unused (leave empty unless you plan a custom mod)

If you change this mapping, you will need to update the controller firmware accordingly.

#### 3.1.3 STEMMA QT (I²C) Connections

The I/O board has STEMMA QT in and out ports.
- The input port is the one adjacent to the small power/status LED on the board.

You will:
- Feed I²C into this port from the last matrix display (Section 3.2).

#### 3.1.4 Placeholder Image – Arcade Button Board

![Arcade Button IO Board Wiring](../assets/placeholder_button_io.jpg)

(Replace with your annotated photo.)

For more details, see Adafruit’s documentation:
- https://learn.adafruit.com/adafruit-led-arcade-button-qt/overview

---

### 3.2 Matrix Display Assembly

Each module consists of:
- One 8×8 LED matrix display
- One matching I²C “backpack” PCB

When ordered from Adafruit, the displays and backpacks must be soldered together.

#### 3.2.1 Orientation and Alignment

Each backpack has a grid of through-holes and one filled-in circle in a corner. That filled dot is the orientation key.

On the LED matrix, one side has printed markings (e.g., part number or “Lucky Light”).

Correct alignment:
- Printed side of the LED matrix should face toward you.
- The filled-in circle on the backpack should align at the bottom-left under the start of the printed text.

Dry-fit everything first; if soldered wrong, the display won’t function.

#### 3.2.2 Soldering Notes
- Many pins: take your time.
- Seat the matrix flush against the backpack.
- Solder 1–2 pins first, check alignment, then finish.

Repeat for all four matrices.

#### 3.2.3 Address Jumper Configuration

On the backpack are three jumpers: A0, A1, A2.

Use this addressing scheme:
- Display 1: No jumpers (all open)
- Display 2: A0
- Display 3: A1
- Display 4: A0 and A1

Leave A2 open on all displays.

Ensure A0 and A1 are separate bridges—do not short them.

#### 3.2.4 Daisy-Chaining the Four Matrices

Each backpack includes STEMMA QT connectors.

Chain order:
1. Controller → Display 1 (input)
2. Display 1 → Display 2
3. Display 2 → Display 3
4. Display 3 → Display 4
5. Display 4 (output) → Arcade Button I/O board (input)

This is the full chain:

Controller → 1 → 2 → 3 → 4 → Button Board

#### 3.2.5 Placeholder Image – Matrix Orientation

![Matrix Backpack Orientation](../assets/placeholder_matrix_orientation.jpg)

(Replace with your actual orientation photo.)

More details:
- https://learn.adafruit.com/adafruit-led-backpack/0-8-8x8-matrix-assembly

---

### 3.3 Testing With Controller Debug Mode

Once everything is wired:
- All four matrices
- Button board
- Controller flashed

Run:
```bash
CMD DEBUG
```

Expected behavior:
- Controller NeoPixel = orange
- Matrices cycle patterns and numbers
- Button LEDs cycle patterns

If any matrix misbehaves, check:
- Orientation
- Address jumpers
- STEMMA cable order
- Connector seating

---

## 4. NeoPixel System

The NeoPixel system consists of six addressable LED strips, each with 8 pixels (48 total). These are driven by a single data line from the controller and powered directly from the UPS 5 V rail.

This section is mostly repetitive connector work—tedious but straightforward.

---

### 4.1 NeoPixel Strip Layout & Pads

Each NeoPixel strip has:
- DIN (data in)
- DOUT (data out)
- VDD (5 V)
- GND (two ground pads)

You will:
- Feed data and power into the first strip only
- Daisy-chain data and power along the remaining strips

---

### 4.2 Adding 3-Pin JST Connectors to Each Strip

You will be soldering 3-pin JST-SM style connectors to each strip for:
- Data
- 5 V
- Ground

#### 4.2.1 Orientation and Placement
1. Choose one side for all connectors so every strip is consistent.
2. Note that each JST connector has:
   - A flush side
   - A side where the plastic housing protrudes more
3. Mount the connectors so the flush side faces the LED side of the strip.

This allows you to later cover the entire strip (including connectors) with clear VHB tape, improving adhesion and light diffusion on the acrylic. If the connectors protrude the wrong way, the tape will not make solid contact with both the strip and the acrylic.

Soldering JSTs is fiddly—take your time. The BOM includes extra connectors in case you need to redo a few.

#### 4.2.2 Placeholder Image – NeoPixel Connector Orientation

![NeoPixel Strip JST Orientation](../assets/placeholder_neopixel_jst.jpg)

(Replace with your annotated NeoPixel photo.)

---

### 4.3 Input Cable: Power + Data from UPS and Controller

The first strip receives:
- 5 V and GND from the UPS (via a 2-pin JST)
- DATA from the controller (GPIO pin 6)

Create a short harness as follows:
- On the strip side, terminate into a 3-pin JST (5 V, data, GND).
- For power:
  - Run a pair of wires to a 2-pin JST that plugs directly into the UPS 5 V output.
- For data:
  - Run a single wire to pin 6 on the controller.

Keep these leads relatively short; all strips live around the display, so long cables are unnecessary and harder to manage.

Use consistent colors, for example:
- Red = 5 V
- Black = GND
- Green = Data

---

### 4.4 Daisy-Chaining the Remaining Strips

For the remaining five strips:
- Connect DOUT of strip N to DIN of strip N+1.
- Carry 5 V and GND through each strip using the JST connectors.

The final layout is a single chain of six strips electrically:

Controller pin 6 → Strip 1 DIN → Strip 2 DIN → Strip 3 DIN → Strip 4 DIN → Strip 5 DIN → Strip 6 DIN

All strips share the same 5 V and GND from the UPS.

---

### 4.5 Testing NeoPixels with Debug Mode

Once all JST connectors are soldered and the first strip is wired to:
- UPS 5 V / GND, and
- Controller pin 6 (data)

Power the system and from a serial terminal connected to the controller, run:
```bash
CMD DEBUG
```

Expected behavior:
- All six NeoPixel strips should cycle through colors and patterns continuously.

If some or all strips are dark or inconsistent:
- Verify 5 V and GND continuity along the chain.
- Confirm data direction (DIN vs DOUT) on each strip.
- Check JST polarity and solder joints.

This is a repetitive, somewhat frustrating step—plan to take breaks and double-check each connection as you go.

---

## 5. Audio System (I2S Bonnet + TT25 Haptics)

This section covers installation of the Adafruit I2S 3W Stereo Speaker Bonnet, wiring of the TT25 haptic transducer, wiring of both speakers, and preparation for the RGB power-button LED.

---

### 5.1 Mounting the I2S Bonnet

The Adafruit Speaker Bonnet includes a 40‑pin header that plugs directly into the Raspberry Pi’s GPIO header.

Because the Pi 5 has a tall heatsink, you must install the 40‑pin extender included in the BOM before placing the bonnet. This raises the bonnet high enough to clear the heatsink.

Steps:
1. Push the 40‑pin extender onto the Pi’s header.
2. Push the I2S bonnet onto the extender.

At this point the bonnet is mechanically mounted and ready for wiring.

---

### 5.2 Adding a 3‑Pin JST Connector (GPIO 4, 6, 8)

You will solder a 3‑pin JST connector directly to the bonnet. This connector will later drive the RGB LED on the power button.

Solder to the following Pi GPIO pins on the bonnet:
- Pin 4 → 5 V
- Pin 6 → GND
- Pin 8 → GPIO (used for color/animation control)

This is the only soldering required for the audio section.

---

### 5.3 Wiring the TT25 Haptic Transducer (Left Channel)

The TT25 tactile transducer acts as the “bass shaker” or haptic feedback motor.
- Connect it to the left channel output of the I2S bonnet.
- Polarity is not critical for this device.
- Keep wire length reasonably short to avoid rattling.

You may add an inline JST connector so the TT25 can be unplugged during maintenance.

---

### 5.4 Wiring the Stereo Speakers (Right Channel)

You will wire both speakers into the right channel of the bonnet.

Recommended:
- Add a JST inline disconnect for each speaker.
- One speaker is mounted on the removable side panel, so a disconnect is essential.

Both speakers share the same right‑channel output.

Wire length can be kept short; this is easier once the cabinet is assembled and you can measure spacing.

---

### 5.5 Software Setup for the I2S Bonnet

Follow Adafruit’s installation instructions:
- https://learn.adafruit.com/adafruit-speaker-bonnet-for-raspberry-pi

Once installed correctly:
- The Pi 5 desktop should show a speaker volume icon.
- If the volume icon appears, the audio card is properly configured.

If you do not see a volume icon, the bonnet is not active in the OS.

---

### 5.6 Power Button LED (Pre‑Wiring)

Do not plug anything into the 3‑pin header yet.

Later, in the final assembly section, you will:
- Run the power button LED’s JST connector
- Plug it directly into the 3‑pin header on pins 4/6/8

This allows the controller to animate the RGB power button during gameplay.

---

## 6. Camera Module

The camera setup is one of the simplest parts of the hardware build.

---

### 6.1 Connecting the Camera

The Raspberry Pi 5 includes two camera/display ribbon connectors. Either will technically work, but for consistency:
- Plug the camera ribbon into the first (left-most) connector.
- Plug the other end of the ribbon into the Pi Camera Module 3.

Make sure the ribbon cable is fully seated and the locking tabs are closed.

---

### 6.2 Installing Camera Libraries

The Pi Camera 3 uses the new rpicam stack. Older tutorials reference libcamera-* commands, but these have been replaced.

Install the camera software if needed:
```bash
sudo apt update
sudo apt install rpicam-apps
```

---

### 6.3 Testing the Camera

Run the updated test command:
```bash
rpicam-hello
```

You should see a 5-second preview window appear, confirming:
- The camera is detected
- The ribbon is connected correctly
- Drivers are functional

If the preview does not appear:
- Reseat the ribbon cable on both ends
- Reboot the Pi
- Confirm the correct connector was used

The camera module is now ready for the software portion of high-score photo capture.

---

## 7. Final Integration & System Test

This final section connects the last two remaining external components:
- The charging input (DC jack → UPS 5V In)
- The RGB power button (momentary switch + NeoPixel LED)

Once these are installed, the entire system can be tested end‑to‑end.

---

### 7.1 Charging Input (DC Jack → UPS 5V In)

You will be using a DC barrel jack cable (female → male). The male end will be cut off and replaced with a 2‑pin JST connector.

Steps
1. Cut off the male barrel connector.
2. Strip back the cable to expose red (5V) and black (GND) wires.
3. Crimp or solder these into a 2-pin JST connector.
4. Plug the JST directly into the UPS 5V IN header.

Important:
Double‑check polarity before connecting. Reversing 5V and GND can short the UPS.

Why we do it this way

The UPS has a barrel jack input, but using it adds extra width that conflicts with the tight interior layout. The JST input keeps everything slim and clean.

When plugged in, the UPS LEDs should indicate charging immediately.

---

### 7.2 Power Button (Momentary Switch + RGB LED)

If you are using the recommended power button, it contains seven wires, split into two groups:
- 2 wires → power‑switch function
- 3 wires → RGB LED
- (and two unused wires depending on manufacturer variant)

Because the colors are non‑standard, follow the mapping exactly.

#### 7.2.1 Switch Function Wires

For the actual momentary switch:
- White = Common (COM)
- Green = Normally Open (NO)

Crimp these into a 2‑pin JST and plug into the UPS header labeled POWER BUTTON.

#### 7.2.2 RGB LED Wires

For the internal RGB LED, the 3‑pin JST must match GPIO pins 4 (5V), 6 (GND), and 8 (DATA) on the bonnet.

Viewed facing the JST header on the bonnet, left‑to‑right pin order is:

Pin 4 (5V)   |   Pin 6 (GND)   |   Pin 8 (DATA)

Match the button’s wires to that exact order:
- Black → 5V (Pin 4)
- Yellow → GND (Pin 6)
- Red → DATA (Pin 8)

So your 3‑pin JST should be arranged left to right like this:

Black   |   Yellow   |   Red  
5V      |   GND      |   DATA

This ensures proper power and signal alignment with the bonnet’s 4/6/8 pin cluster.

#### 7.2.3 Recommended Wiring Strategy

Because the button is mounted on the front panel:
- Add a 5-pin disconnect near the button.
- Split this 5‑pin harness into:
  - 2‑pin JST → UPS (switch)
  - 3‑pin JST → Bonnet (RGB LED)

This makes future servicing dramatically easier.

---

### 7.3 Final System Test Checklist

Once all wiring is complete, the following items should be verified:

#### 7.3.1 Controls
- Left button → left flipper + menu navigation
- Right button → right flipper + menu navigation
- Center button → launch ball + confirm menu options

#### 7.3.2 Visual Outputs
- All four matrix displays update during gameplay
- NeoPixel strips animate correctly
- Button LEDs light and cycle
- Power button RGB responds to scripts / game state

#### 7.3.3 Audio & Haptics
- Speakers output clean sound
- TT25 haptic transducer vibrates during flipper hits and impacts

#### 7.3.4 Power Behavior
- Plugging in the DC jack shows charging LEDs on the UPS
- System shuts down cleanly
- With EEPROM POWER_OFF_ON_HALT=1, the UPS fully powers off

#### 7.3.5 Camera
- Achieving a top‑3 score triggers the camera
- The captured image appears in‑game

---

### 7.4 Ready for Cabinet Installation

If:
- All sections above pass testing
- Software and controller firmware are loaded
- Cabinet panels are completed

…you can now mount all electronics into the cabinet, route cables neatly, close the enclosure, and begin full play.

Your Mini Pinball Machine hardware is now fully assembled.

---
