# Mini Pinball Machine
<a href="https://www.buymeacoffee.com/maxrfinch" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/default-orange.png" alt="Buy Me A Coffee" height="41" width="174"></a>

## Project Overview

This fork is intended as a full modernization of the original Chris Dalke mini pinball machine. While not necessarily easier to build, this version focuses on cleaner wiring, a fully self‑contained power system (no external wall supply), tighter component packaging, and vastly improved maintainability.

The original project did not include 3D body/cabinet files or controller firmware. This fork includes both — along with upgraded physics, cabinet files designed around 3 mm SendCutSend acrylic, and a rewritten controller system based on the Adafruit KB2040.

### Background & Inspiration

I came across the Chris Dalke project while searching for my first Raspberry Pi project. My girlfriend and I love pinball machines, often ending up at our local pinball bar every other weekend. Building a mini pinball machine as a first Pi project was ambitious, but that challenge is what made the idea exciting.

During development, this fork evolved into something much more expansive: porting the physics engine, writing the full KB2040 controller firmware, refactoring the codebase, and designing a complete physical cabinet with modern fabrication options.

Inspired by real machines — especially the Elton John machine by Jersey Jack — I pushed the immersiveness further by adding RGB lighting, illuminated buttons, optional haptics, and even a camera that can capture high‑score images.

This Fork is updated to include:
- Ported Chipmunk physics -> Box2D for more reliable gameplay in 2025+
- 3D Print Files
- 3D Laser Cut Files *or* SendCutSend order links
- Full Build Instructions

Optional:
- This build includes the ability to add up to 48 NEOPIXEL RGB leds for light effects
- This build includes the ability to add LED arcade buttons
- This build includes the ability to add a camera for high score pictures (one of my favorite features of the Elton John pinball machine)


**This is a fork and upgrade over the original repository**
- Pinball Arcade Inspiration: https://www.chrisdalke.com/projects/mini-pinball-machine/
- Chris Dalke: https://www.chrisdalke.com/

### 
Full acrylic cabinet build instructions, alignment tips, 3D files, and SendCutSend notes:  
`/docs/case_build.md`

## Bill of Materials (BOM)

A complete, always‑up‑to‑date Bill of Materials for this project is maintained in a Google Sheet.

**BOM Link:**  
https://docs.google.com/spreadsheets/d/1uvjGl2vRcwuacQ0akA2e_Oj9aA8tAHwuTviBW_zliok/edit?gid=0#gid=0

The sheet includes:
- Core required electronics  
- Cabinet materials (3 mm acrylic, hardware, fasteners)  
- UPS, power routing, battery options  
- LED and button components  
- Audio / haptics (bonnet, transducers, drivers)  
- Tools and recommended alternates  
- Optional upgrades (camera, additional LEDs, custom wiring harnesses)

This BOM is the source of truth for quantities, vendor links, part swaps, and total cost.

## Instructions

**I recommend obtaining all your hardware and parts at once. This will make the build process enjoyable and fast. However, if this is not plausible - I recommend obtaining hardware first, then your cabinet so you can iterate and play with the code.**

### 1) [Hardware / Electronics Build](/documentation/hardware_instructions.md)
Start by assembling your hardware first. This step requires soldering and assembling connectors & components. Please take your time.

### 2) [Software / Firmware Setup](/documentation/software_setup.md)
Raspberry Pi 5 & KB 2040 setup, building code.

### 3) [Case / Cabinet Build](/documentation/cabinet_instructions.md)
Includes 3D print instructions, Acrylic Welding, etc. Take your time!

### Notes
**Geekworm X1206 Voltage Warning:**
Perform the following on your PI:
1) From the command line start by running the following command: ```sudo rpi-eeprom-config -e```
2) Add ``PSU_MAX_CURRENT=5000`` at the end of what shows up, likely after ``BOOT_ORDER0xf41``
3) Reboot your PI ```sudo reboot```
