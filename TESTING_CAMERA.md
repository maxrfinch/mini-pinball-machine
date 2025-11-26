# Testing Camera Functionality on Raspberry Pi

## Prerequisites

### Hardware
- Raspberry Pi 4 or 5
- Pi Camera Module 3 (connected via ribbon cable)
- KB2040 firmware controller (for LED effects)

### Software
```bash
# Install libcamera and dependencies
sudo apt update
sudo apt install -y libcamera-dev libcamera-tools libjpeg-dev

# Verify camera is detected
libcamera-hello --list-cameras
```

## Building with Camera Support

```bash
cd /path/to/mini-pinball-machine

# Create build directory
mkdir -p build
cd build

# Configure CMake (will auto-detect Raspberry Pi)
cmake ..

# Build
make -j$(nproc)
```

The build system will automatically:
- Detect Raspberry Pi via `/proc/device-tree/model`
- Enable `PLATFORM_RPI` define
- Link libcamera and libcamera-base
- Include camera C++ wrapper

## Testing the Camera

### 1. Verify Camera Capture Works
Before running the game, test camera capture independently:

```bash
# Test 150x150 square capture
libcamera-still --immediate --nopreview --timeout 1 \
  --width 150 --height 150 \
  --roi 0.125,0.125,0.75,0.75 \
  -o test_photo.jpg

# Verify the file was created
ls -lh test_photo.jpg
```

### 2. Run the Game
```bash
cd build
./pinball
```

### 3. Trigger Top-3 Game Over Scene
To reach the camera photo scene:
1. Start a game (press CENTER button)
2. Play until game over
3. Achieve a score in the top 3
4. The Top-3 Game Over scene will load automatically

### 4. Observe Camera Behavior

#### Expected Behavior:
1. **Scene Entry**
   - 150×150 preview box appears (centered horizontally, Y=325)
   - NeoPixel LEDs turn off (`CMD NEO EFFECT NONE`)
   - Preview shows "LOADING..." placeholder (live preview not yet implemented)

2. **Name Entry**
   - Use LEFT/RIGHT buttons to select character position
   - Use CENTER button to cycle through letters (A-Z, space)
   - Preview box remains visible throughout

3. **Score Submission**
   - Move selector to 6th position (submit arrow)
   - Press CENTER button
   - **Camera flash**: NeoPixel board 3 flashes white
   - **200ms delay**
   - Photo captured to `Resources/Photos/<name>_<score>.png`
   - LEDs return to ATTRACT mode
   - Transition back to menu

#### Verify Photo Capture:
```bash
# Check photos directory
ls -lh Resources/Photos/

# Example output:
# -rw-r--r-- 1 pi pi 12345 Nov 26 16:00 ALICE_50000.png
# -rw-r--r-- 1 pi pi 13456 Nov 26 16:05 BOB___75000.png
```

### 5. Check Console Output

Look for these log messages:

```
CAMERA: Initialized successfully via libcamera
CAMERA: Preview started
CAMERA: Photo saved to Resources/Photos/ALICE_50000.png
CAMERA: Preview stopped
CAMERA: Shutdown complete
```

### 6. Test Error Handling

#### Camera Not Connected:
- Game should start normally
- Top-3 scene shows "NO CAMERA" in preview box
- No crash or hang
- Score submission works without photo

#### Camera Initialization Fails:
- Check logs for "CAMERA: Failed to initialize libcamera"
- Game continues without camera features

## Troubleshooting

### Camera Not Detected
```bash
# Check camera connection
vcgencmd get_camera

# Should show: supported=1 detected=1

# If not detected, check ribbon cable connection
# and ensure camera is enabled in raspi-config
```

### libcamera-still Not Found
```bash
# Install libcamera tools
sudo apt install libcamera-apps

# Verify installation
which libcamera-still
```

### Permission Issues
```bash
# Ensure user is in video group
sudo usermod -a -G video $USER

# Log out and back in for changes to take effect
```

### Photos Not Saving
```bash
# Check directory permissions
ls -ld Resources/Photos/

# Create directory manually if needed
mkdir -p Resources/Photos
chmod 755 Resources/Photos
```

### NeoPixel Effects Not Working
```bash
# Check serial connection to KB2040
ls -l /dev/ttyACM0

# Verify firmware is running
screen /dev/ttyACM0 115200

# Type: CMD PING
# Should see: EVT PONG
```

## Known Limitations

1. **Live Preview**: Not functional yet
   - Shows "LOADING..." placeholder
   - Photo capture still works
   - Future: Will show real-time camera feed

2. **CAMERA_PREVIEW Effect**: Not defined in firmware
   - Currently uses `CMD NEO EFFECT NONE` (LEDs off)
   - Consider adding to firmware if desired

3. **External Command**: Uses `libcamera-still`
   - Reliable but spawns external process
   - Future: Native C++ API integration

## Development Notes

### Adding Live Preview
To implement live preview, complete these TODOs in `src/camera_libcamera.cpp`:
1. Implement proper request/completion handling
2. Add frame buffer mapping
3. Convert frame format (RGB/YUV)
4. Update Raylib texture with frame data

### Filename Sanitization
Names with spaces are converted to underscores:
- Input: "AL CE" → File: "AL_CE_50000.png"
- Input: "  BOB" → File: "___BOB_50000.png"

### Performance
- Photo capture: ~1-2 seconds (includes 200ms flash delay)
- Does not block game during capture
- Preview update: N/A (placeholder only)

## Success Criteria

✅ Game starts without errors on Pi with camera  
✅ Game starts without errors on Pi without camera  
✅ Top-3 scene displays preview box  
✅ Photo captured on score submission  
✅ Photo file saved with correct naming  
✅ NeoPixel flash triggers correctly  
✅ No crashes or hangs during camera operations  

---

For issues or questions, check the main README.md or CAMERA_IMPLEMENTATION.md.
