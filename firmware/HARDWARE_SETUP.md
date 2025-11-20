# Hardware Setup Guide

This document provides detailed hardware connection information for the Pico pinball controller.

## Required Components

1. **Raspberry Pi Pico** (or Pico W)
2. **6× WS2812B LED Boards** (8 LEDs each = 48 total)
3. **Adafruit LED Arcade Button 1×4 I2C Breakout** (Arcade Seesaw board, Adafruit 5296)
4. **4× Adafruit 1.2" 8x8 LED Matrix with I2C Backpack** (HT16K33, Adafruit 1855)
5. **2× Adafruit DRV2605L Haptic Motor Controller**
6. **3× LED Arcade Buttons** (30mm translucent)
7. **2× Haptic Motors** (ERM or LRA)
8. **5V Power Supply** (for NeoPixels, 3A minimum)
9. **3.3V Power Supply** (for Pico and I²C devices)

## Wiring Diagram

### Power Distribution

```
5V Power Supply
├─→ NeoPixel LED Boards (VIN)
└─→ (Optional) USB power for Pico

3.3V Source (from Pico or external)
├─→ Arcade Seesaw Button Board (VIN)
├─→ 4× Matrix Displays with I2C Backpacks (VIN)
├─→ 2× DRV2605L Boards (VIN)
└─→ Haptic Motors (through DRV2605L)
```

**Important:** NeoPixels require 5V. All I²C devices operate at 3.3V logic levels.

### NeoPixel Connection

```
Pico GPIO2 (DATA OUT)
    ↓
330Ω Resistor (recommended)
    ↓
Board 1 (Right Front) → Board 2 (Right Rear) → Board 3 (Camera Bar)
    ↓                       ↓                       ↓
Board 4 (Left Rear) → Board 5 (Left Front) → Board 6 (Front Bar)
```

**NeoPixel Power Notes:**
- Add 1000µF capacitor across 5V and GND near first LED board
- Keep data line short or use shielded cable
- Each LED draws ~60mA at full white
- 48 LEDs × 60mA = 2.88A maximum current

### I²C0 Bus (Arcade Seesaw Buttons + Matrix Displays)

```
Pico GPIO4 (SDA0) ──┬─→ Arcade Seesaw (SDA) [0x30]
                    ├─→ Matrix 1 w/ Backpack (SDA) [0x70]
                    ├─→ Matrix 2 w/ Backpack (SDA) [0x71]
                    ├─→ Matrix 3 w/ Backpack (SDA) [0x72]
                    └─→ Matrix 4 w/ Backpack (SDA) [0x73]

Pico GPIO5 (SCL0) ──┬─→ Arcade Seesaw (SCL)
                    ├─→ Matrix 1 w/ Backpack (SCL)
                    ├─→ Matrix 2 w/ Backpack (SCL)
                    ├─→ Matrix 3 w/ Backpack (SCL)
                    └─→ Matrix 4 w/ Backpack (SCL)

Pull-up: 4.7kΩ resistors on SDA0 and SCL0 to 3.3V
```

### I²C0 Bus - Right Haptic (Shared with buttons and displays)

```
Pico GPIO4 (SDA0) ──┬─→ Arcade Seesaw (SDA) [0x30]
                    ├─→ Matrix 1-4 w/ Backpacks (SDA) [0x70-0x73]
                    └─→ DRV2605L Right (SDA) [0x5A]

Pico GPIO5 (SCL0) ──┬─→ Arcade Seesaw (SCL)
                    ├─→ Matrix 1-4 w/ Backpacks (SCL)
                    └─→ DRV2605L Right (SCL)

Pull-up: 4.7kΩ resistors on SDA0 and SCL0 to 3.3V
```

### I²C1 Bus (Left Haptic Only)

```
Pico GPIO6 (SDA1) ───→ DRV2605L Left (SDA) [0x5A]

Pico GPIO7 (SCL1) ───→ DRV2605L Left (SCL)

Pull-up: 4.7kΩ resistors on SDA1 and SCL1 to 3.3V
```

**Note:** The matrix displays use daisy-chained I²C backpacks (HT16K33), each with a unique address configured via solder jumpers. They share the I²C0 hardware bus with the Arcade Seesaw buttons. DRV2605L haptic controllers have a fixed I²C address of 0x5A, so the two haptic modules must be on separate I²C buses (left on I²C1, right on I²C0). GPIO8/9 are now available for other uses.

### Button Connections

Buttons connect to the Arcade Seesaw board:
- **Button 0** (Left) → Left Flipper
- **Button 1** (Center) → Start/Menu
- **Button 2** (Right) → Right Flipper
- **Button 3** → Reserved (not used)

### Status LEDs

```
Pico GPIO12 → Status LED (heartbeat)
Pico GPIO13 → Mode LED
```

Use 220Ω current-limiting resistors for LEDs.

## Physical Layout

### LED Board Positioning (Player's View)

```
        [Board 3: Camera Bar]
              (Top)

[Board 4]           [Board 1]
Left Rear           Right Front
   ↓                    ↓
[Board 5]           [Board 2]
Left Front          Right Rear

        [Board 6: Front Bar]
            (Bottom)
```

### LED Indexing

- **Board 1** (Right Front): LEDs 0-7, direction: away from player
- **Board 2** (Right Rear): LEDs 8-15, direction: away from player
- **Board 3** (Camera Bar): LEDs 16-23, direction: right to left
- **Board 4** (Left Rear): LEDs 24-31, **REVERSED** (mirrored to Board 2)
- **Board 5** (Left Front): LEDs 32-39, **REVERSED** (mirrored to Board 1)
- **Board 6** (Front Bar): LEDs 40-47, direction: left to right

## I²C Address Configuration

### Setting HT16K33 Addresses

The 4 matrix displays need different I²C addresses. Configure using solder jumpers:

| Display | Address | Jumpers       |
|---------|---------|---------------|
| 1       | 0x70    | None (OPEN)   |
| 2       | 0x71    | A0            |
| 3       | 0x72    | A1            |
| 4       | 0x73    | A1 + A2       |

### Setting DRV2605L Configuration

This project uses two DRV2605L haptic modules, both at I²C address 0x5A. While the DRV2605L chip has an ADDR pin to change the address to 0x5B, this design connects the modules to separate I²C buses instead:
- **Left Module**: I²C1 bus (GPIO6/7) at address 0x5A
- **Right Module**: I²C0 bus (GPIO4/5) at address 0x5A (shared with buttons and displays)

This approach avoids needing to modify the ADDR pin on either module and allows both to use the default address.

## Testing Connections

### Using I²C Scanner

Before flashing firmware, verify all I²C devices are detected:

```bash
# On Linux/Raspberry Pi with i2c-tools
i2cdetect -y 0  # Check I²C0 bus
i2cdetect -y 1  # Check I²C1 bus
```

Expected devices:
- **I²C0:** 0x30 (Arcade Seesaw Buttons), 0x5A (Right Haptic), 0x70-0x73 (Matrix Displays)
- **I²C1:** 0x5A (Left Haptic)

### Quick Function Tests

1. **NeoPixels**: All LEDs should light up in debug mode after 30s
2. **Buttons**: Pressing each button should trigger haptic feedback
3. **Displays**: Score should appear right-aligned on matrices
4. **Haptics**: Each button press should produce tactile feedback

## Common Issues

### NeoPixels Not Working
- Check 5V power supply capacity (need 3A minimum)
- Verify data line is connected to GPIO2
- Add 330Ω resistor in series with data line
- Add capacitor across power rails
- Check for continuity in daisy chain

### I²C Devices Not Detected
- Verify pull-up resistors (4.7kΩ) on SDA and SCL
- Check power supply to all devices (3.3V)
- Verify correct I²C addresses
- Check for loose connections
- Try reducing bus speed in code

### Buttons Not Responsive
- Verify Arcade Seesaw board is at address 0x30
- Check button wiring to Arcade Seesaw board
- Ensure buttons are normally-open (NO) type
- Verify pull-up configuration in firmware

### Haptics Not Working
- Check DRV2605L address configuration
- Verify motors are connected to +/- terminals
- Test with a simple vibration waveform
- Check motor voltage requirements

### Display Flickering
- Verify stable 3.3V power supply
- Check I²C bus for noise/interference
- Try reducing display update rate
- Ensure proper ground connections

## Safety Notes

1. **Never exceed 3.3V on Pico GPIO pins**
2. **Use separate power supplies for 5V (NeoPixels) and 3.3V (logic)**
3. **Always use current-limiting resistors for LEDs**
4. **Add fuses to protect against shorts**
5. **Ensure proper heat dissipation for power regulators**
6. **Use shielded cables for long wire runs**

## Bill of Materials (BOM)

| Component | Quantity | Notes |
|-----------|----------|-------|
| Raspberry Pi Pico | 1 | Or Pico W |
| WS2812B LED Board | 6 | 8 LEDs each |
| LED Arcade Button 1×4 I2C Breakout | 1 | Adafruit 5296 |
| 1.2" 8x8 LED Matrix w/ Backpack | 4 | Adafruit 1855 |
| DRV2605L Haptic | 2 | Adafruit 2305 |
| LED Arcade Buttons | 3 | 30mm or 24mm |
| ERM Motors | 2 | 3V vibration motors |
| 4.7kΩ Resistors | 4 | I²C pull-ups |
| 220Ω Resistors | 2 | Status LEDs |
| 330Ω Resistor | 1 | NeoPixel data line |
| 1000µF Capacitor | 1 | NeoPixel power |
| 5V Power Supply | 1 | 3A minimum |
| Jumper Wires | Assorted | 22-24 AWG |
| Heat Shrink Tubing | Assorted | For connections |

## Tools Needed

- Soldering iron and solder
- Wire strippers
- Multimeter
- Heat gun (for heat shrink)
- Helping hands/PCB holder
- Flush cutters
- Small screwdrivers

## Assembly Tips

1. **Test each component individually** before final assembly
2. **Use color-coded wires** for power and signals
3. **Label all connections** for troubleshooting
4. **Keep I²C wires short** to reduce noise
5. **Secure connections** with heat shrink or electrical tape
6. **Test continuity** before applying power
7. **Start with low brightness** for NeoPixels during testing
8. **Document your specific wiring** for future reference

## Next Steps

After hardware is assembled:
1. Flash the firmware (see README.md)
2. Verify all devices respond in debug mode
3. Test each button for events and haptics
4. Verify LED effects display correctly
5. Test score and ball display on matrices
6. Calibrate timing and brightness as needed
