# MiniPinball Project — Parts List & Overview

## Overview
MiniPinball is a modernized fork of the original **ChrisDalke Mini Pinball Project**, preserving its core gameplay features while expanding the hardware, enclosure, lighting, and interactive elements. This version introduces:

- Raspberry Pi 5 + Raspberry Pi Pico architecture
- Camera-based high‑score capture inspired by Jersey Jack’s *Elton John* machine
- Extensive RGB LED effects using WS2812 NeoPixels
- LED‑lit arcade buttons with full I2C control
- Battery‑powered wireless play via a Geekworm X1206 21700 power system
- Fully acrylic laser‑cut enclosure with 3D‑printed standoffs
- Support for haptic feedback motors (2 zones)
- Forward‑compatible accessory design

This document lists every major component used so far and includes placeholders for laser‑cut files, 3D‑printed models, and future SendCutSend ordering links.

---

## Enclosure & Fabrication

### Fabrication Files (STL-Only)
All project files are provided as **STL models**. This means:

- You can **3D print every component**, including the enclosure walls.
- You can also **upload the STL files to SendCutSend** and order the acrylic parts pre‑cut.
- All wall components are designed for **3 mm acrylic**.

A **SendCutSend order link** will be added here once finalized. Ordering through this link supports further development of MiniPinball.

**TODO:** Add SCS order link.

---

**Materials**
- Acrylic sheets (foggy white for base, colored side walls)
- 3D‑printed standoffs (M5 heat‑set inserts)

**Tools & Consumables (Recommended)**
- Weld‑On 4 acrylic solvent cement *(fine‑tip application recommended)*
- 22‑gauge 5ml lab needles (for Weld‑On)
- 20‑gauge wire (general wiring)
- Zip ties & heat shrink tubing
- M5‑0.8 × 14 mm screws
- M5 threaded inserts (for printed standoffs)
- JST‑XH 2‑pin connectors
- DuPont jumper wires

**3D & Laser‑Cut Files**
- **TODO:** Add STL files for brackets, standoffs, button supports
- **TODO:** Add DXF/SVG files for SendCutSend laser‑cut parts
- **TODO:** Add SendCutSend order link for pre‑cut acrylic kits (affiliate)

---

## Electronics

### Core Compute
- Raspberry Pi 5 — 4 GB RAM  
  https://www.adafruit.com/product/5812
- Aluminum Passive Heatsink Case for Pi 5 *(no fan to reduce power draw)*  
  https://www.adafruit.com/product/6307
- Raspberry Pi Pico H (headers included)  
  https://www.adafruit.com/product/5525

### Power
- [Geekworm X1206 21700 Battery System](https://geekworm.com/products/x1206) (1)
- [Samsung 50E 21700 Cells — 5000mAh](https://imrbatteries.com/products/samsung-50e-21700-5000mah-9-8a-battery) (4)
- [DC Barrel Jack Panel Extension (30 cm)](https://www.adafruit.com/product/5607)

- Geekworm X1206 21700 Battery System *(not listed but required)*
- DC Barrel Jack Panel Extension (30 cm)  
  https://www.adafruit.com/product/5607

---

## Audio
- Stereo Enclosed 3W 4Ω Speaker Set  
  https://www.adafruit.com/product/1669
- Adafruit I2S 3W Class D Amplifier — MAX98357A (x2)  
  https://www.adafruit.com/product/3006

---

## Display & Input
### LED Matrix Score Displays
- Small 1.2" 8×8 Ultra‑Bright Yellow LED Matrix + Backpack (x4)  
  https://www.adafruit.com/product/1855

### Touch Display
- Waveshare 7" Capacitive Touch LCD  
  https://www.amazon.com/dp/B015E8EDYQ

### Camera
- Raspberry Pi Camera Module 3 (120° Wide Angle)  
  https://www.adafruit.com/product/5658

### Arcade Controls
- Arcade Button w/ LED — 30mm Translucent Red (x2)  
  https://www.adafruit.com/product/3489
- Arcade Button w/ LED — 30mm Translucent Blue (x1)  
  https://www.adafruit.com/product/3490
- LED Arcade Button Quick‑Connect Wires (10‑pack)  
  https://www.adafruit.com/product/1152
- Adafruit LED Arcade Button 1×4 I2C Breakout  
  https://www.adafruit.com/product/5296
- ChromaTek RGB Momentary Pushbutton (power button, addressable LED)  
  https://www.amazon.com/dp/B0988YTLNL

---

## Lighting (Neopixels)
- WS2812B LED Stick — 8 LEDs per stick (10‑pack)  
  https://www.amazon.com/dp/B0BWH95XSH

---

## Haptics
- Vibrating Mini Motor Disc (x2)  
  https://www.adafruit.com/product/1201
- Adafruit DRV2605L Haptic Motor Controller (x2)  
  https://www.adafruit.com/product/2305

---

## Connectors, Fasteners & Small Parts
- JST‑XH 2‑Pin Connectors (5 pairs)  
  https://www.amazon.com/dp/B07YKHV46N
- Weld‑On 4 Acrylic Cement (4 oz)  
  https://www.amazon.com/dp/B00TCUJ6AE
- 5‑Pack of 22‑Gauge Applicator Needles  
  https://www.amazon.com/dp/B0CHB275HP
- M3 Tap & Drill Set  
  https://www.amazon.com/dp/B0D6BVN3NY
- M5‑0.8 × 14 mm Screws (20‑pack)  
  https://www.amazon.com/dp/B0C1SFW5HM
- M5 Heat‑Set Threaded Inserts for Plastic (100‑pack)  
  https://www.amazon.com/dp/B0DHGCQTK5

---

## Future Additions (TODO)
- Add full wiring diagram
- Add Pico firmware documentation
- Add Pi 5 software setup guide
- Add LED animation spec
- Add haptic feedback mapping
- Add camera capture module documentation
- Add gameplay logic overview
- Add bill of materials pricing

---

## Licensing
**TODO:** Add project license.

---

## Credits
- Original project by **ChrisDalke**
- This fork by **Max**

---

## Project Status
Active development — major feature additions in progress.

More sections will be added as hardware and software components complete.

