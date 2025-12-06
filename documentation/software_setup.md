# Mini Pinball Machine — Raspberry Pi 5 Setup Guide (Trixie + Wayland)

This README documents the complete, validated setup required to build and run the **Mini Pinball Machine** on a **Raspberry Pi 5** running **Debian Trixie** using the **Wayland** display stack. It includes:

- Graphics & library setup (raylib + Box2D)
- CPU underclocking configuration
- Audio system setup (section stub—awaiting user details)
- Camera (libcamera) setup
- Source installation, build, and run instructions for the game

Sections are structured for clarity and can be expanded individually.

---

# 1. System Preparation

```bash
sudo apt update
sudo apt install -y git build-essential cmake pkg-config \
  libx11-dev libxrandr-dev libxi-dev libxinerama-dev libxcursor-dev \
  libgl1-mesa-dev mesa-utils wayland-protocols libwayland-dev \
  libasound2-dev libpulse-dev
```

These packages provide:

- Mesa OpenGL drivers
- X11 compatibility libraries (needed for raylib's GLFW backend under Wayland)
- Wayland dev support
- Audio backend development headers

---

# 2. Install Box2D (3.1.1)

Verified, stable, and used by the pinball physics engine.

```bash
cd ~
git clone https://github.com/erincatto/box2d.git
cd box2d
git checkout v3.1.1

mkdir build
cd build

cmake .. \
  -DBOX2D_BUILD_UNIT_TESTS=OFF \
  -DBOX2D_BUILD_TESTBED=OFF \
  -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)
sudo make install
sudo ldconfig
```

This installs Box2D to `/usr/local`. If using a local build path, pass:

```bash
-DBOX2D_ROOT=~/box2d/build
```

to your pinball CMake configuration.

---

# 3. Install raylib (Stable) with OpenGL 2.1

## Why OpenGL 2.1?

The Raspberry Pi 5 / Mesa driver exposes:

```
OpenGL version string: 3.1 Mesa …
```

raylib defaults to **OpenGL 3.3 core**, which results in:

- `GLXBadFBConfig`
- `GLFW error 65543`
- Window/context creation failures

Building raylib for **OpenGL 2.1** avoids this incompatibility and has been verified as stable.

## Build Steps

```bash
cd ~
git clone https://github.com/raysan5/raylib.git
cd raylib

git fetch --all --tags
git checkout 5.5.0        # or latest stable tag

cd src
make clean

make PLATFORM=PLATFORM_DESKTOP GRAPHICS=GRAPHICS_API_OPENGL_21
sudo make install
sudo ldconfig
```

Installs to:

- `/usr/local/include/raylib.h`
- `/usr/local/lib/libraylib.*`

raylib will then be auto-detected by the pinball project's CMake configuration.

---

# 4. Underclocking the Raspberry Pi 5

To keep power draw + heat manageable inside the mini pinball cabinet, the Pi 5 can be safely underclocked while maintaining stable performance for the game.

Edit the firmware configuration:

```bash
sudo nano /boot/firmware/config.txt
```

Recommended settings:

```ini
arm_freq=1200
arm_boost=0
```

You may also experiment with:

```ini
over_voltage=0
```

Reboot:

```bash
sudo reboot
```

Check clocks:

```bash
vcgencmd measure_clock arm
```

---

# 5. Audio Setup (Adafruit I2S Bonnet)

The Adafruit MAX98357A / I2S Stereo Bonnet requires several configuration steps on Raspberry Pi OS Trixie. These steps configure I2S, disable onboard audio, and install Adafruit’s I2S amplifier support script.

## 5.1 Disable onboard audio

```bash
sudo nano /boot/firmware/config.txt
```

Scroll to the **bottom** and locate:

```
dtparam=audio=on
```

Comment it out:

```
# dtparam=audio=on
```

Save and exit.

---

## 5.2 Create a Python virtual environment

The Adafruit installer expects a working Python environment with system site packages exposed.

Install venv support:

```bash
sudo apt install -y python3-venv
```

Create the venv:

```bash
python3 -m venv env --system-site-packages
```

Activate:

```bash
source env/bin/activate
```

---

## 5.3 Install installer dependencies & run the Adafruit I2S amplifier script

```bash
sudo apt install -y wget
pip3 install adafruit-python-shell
wget https://github.com/adafruit/Raspberry-Pi-Installer-Scripts/raw/main/i2samp.py
sudo -E env PATH=$PATH python3 i2samp.py
```

Follow the script prompts, then reboot.

---

# 6. Camera Support (libcamera)

For photo capture / high score images / gameplay overlays.

Install libcamera components:

```bash
sudo apt install -y libcamera-apps libcamera-dev libjpeg-dev
```

Check that your camera is detected:

```bash
rpicam-hello
```

Optional tuning file installation will depend on your camera sensor module.

---

# 7. Clone & Build the Game

```bash
cd ~
git clone https://github.com/maxrfinch/mini-pinball-machine.git
cd mini-pinball-machine
```

Build directory:

```bash
rm -rf build
mkdir build
cd build
```

Run CMake:

```bash
cmake .. -DBOX2D_ROOT=~/box2d/build
make -j$(nproc)
```

Run the game (from the Pi desktop session):

```bash
./pinball
```

Running over SSH will **not** work because a GPU context must be created on the physical display session.

---

# 8. NeoPixel + Game Launch Integration

The Mini Pinball Machine uses a WS2812B NeoPixel LED strip for visual effects. To integrate NeoPixel control with the game launch process, follow these steps:

## Hardware Setup

- Connect the NeoPixel data line to a suitable GPIO pin on the Raspberry Pi (e.g., GPIO18).
- Provide a stable 5V power supply for the LED strip.
- Ensure a common ground between the Pi and the LED power supply.

## Software Setup

Install the `rpi_ws281x` library for NeoPixel control:

```bash
sudo pip3 install rpi_ws281x adafruit-circuitpython-neopixel
sudo python3 -m pip install --force-reinstall adafruit-blinka
```

Enable PWM and SPI interfaces if necessary:

```bash
sudo raspi-config
# Navigate to Interface Options and enable PWM and SPI
```

## Integration with Game Launch

Create a Python script `neopixel_launch.py` to initialize the LED strip and perform a startup animation before launching the game:

```python
import time
import board
import neopixel
import subprocess

# Configure NeoPixel strip
pixel_pin = board.D18
num_pixels = 30
ORDER = neopixel.GRB

pixels = neopixel.NeoPixel(pixel_pin, num_pixels, brightness=0.3, auto_write=False, pixel_order=ORDER)

def startup_animation():
    for i in range(num_pixels):
        pixels[i] = (255, 0, 0)  # Red color
        pixels.show()
        time.sleep(0.05)
    time.sleep(1)
    pixels.fill((0, 0, 0))
    pixels.show()

if __name__ == "__main__":
    startup_animation()
    # Launch the pinball game
    subprocess.run(["/home/pi/mini-pinball-machine/build/pinball"])
```

Make the script executable:

```bash
chmod +x neopixel_launch.py
```

Run this script instead of the game binary directly to have the NeoPixel animation on startup.

## Autostart Configuration

To automatically run the NeoPixel launch script on boot within the desktop session, add it to the LXDE autostart file:

```bash
nano ~/.config/lxsession/LXDE-pi/autostart
```

Add the line:

```
@/usr/bin/python3 /home/pi/mini-pinball-machine/neopixel_launch.py
```

Save and reboot.

---

This integration provides visual feedback during game startup and enhances the user experience of the Mini Pinball Machine.
