# Camera Implementation for Top-3 Game Over Scene

## Overview
This implementation adds camera functionality to the Top-3 Game Over scene, allowing players to capture a photo when they achieve a top-3 score.

## Architecture

### Platform Support
- **Raspberry Pi**: Full camera support using Pi Camera Module 3 via libcamera command-line tools
- **Other Platforms**: No-op stubs (camera disabled cleanly)

### Components

1. **camera.h**: C interface for camera operations
2. **camera_libcamera.hpp/cpp**: C++ wrapper around libcamera API (Pi only, partial implementation)
3. **camera_wrapper.cpp**: C-to-C++ bridge that exposes camera functionality to C code

### Key Features

- **Live Preview**: 150×150 pixel square preview box
  - Position: Centered horizontally, vertical center at Y=325
  - Square-cropped before scaling
  - Updates during name entry
  - Uses libcamera-vid MJPEG stream for real-time preview

- **Photo Capture**: 
  - Triggered on score submission (pressing enter on 5th character)
  - Filename format: `Resources/Photos/<username>_<score>.png`
  - Captures at 640x480, crops to square, resizes to 150x150
  - Saves as PNG format
  - Coordinated with NeoPixel flash effect

- **NeoPixel Integration**:
  - Preview mode: `CMD NEO EFFECT NONE` (LEDs off for photo)
  - Flash: `CMD NEO EFFECT CAMERA_FLASH` (board 3 white flash)
  - Restore: `CMD NEO EFFECT ATTRACT` (return to idle)

## Usage Flow

1. **Enter Top-3 Game Over Scene**
   - Camera initialized (if available)
   - Preview starts (libcamera-vid MJPEG stream)
   - NeoPixels set to NONE mode

2. **Name Entry**
   - Live preview displayed in 150×150 box
   - Preview frames captured and displayed in real-time
   - Player enters name using left/right/center buttons

3. **Submit Score**
   - Trigger NeoPixel flash
   - Wait 200ms for flash
   - Stop preview (release camera)
   - Capture photo using libcamera-still
   - Convert to PNG format
   - Restore LEDs to ATTRACT mode

## Build Configuration

### CMakeLists.txt Changes
- Enabled C++17 compilation
- Added camera source files
- Conditional libcamera linking on Raspberry Pi
- Detects Pi via `/proc/device-tree/model`

### Required Libraries (Pi only)
- libcamera
- libcamera-base
- libjpeg (for future JPEG encoding)

## Error Handling
- Camera initialization failure: Skip camera, continue gameplay
- Photo capture failure: Log error, continue gameplay
- Platform without camera: All functions are no-ops

## File Structure
```
src/
├── camera.h                 # C API interface
├── camera_libcamera.hpp     # C++ libcamera wrapper (Pi only)
├── camera_libcamera.cpp     # Implementation (Pi only)
└── camera_wrapper.cpp       # C-to-C++ bridge
```

## Implementation Details

### Live Preview
The live preview uses `libcamera-vid` to capture MJPEG frames:
1. A named pipe is created for inter-process communication
2. `libcamera-vid` is spawned as a child process outputting MJPEG to the pipe
3. The main application reads JPEG frames from the pipe
4. Each frame is decoded, cropped to square, resized to 150x150
5. The texture is updated for display

### Photo Capture
Photo capture uses `libcamera-still`:
1. Preview is temporarily stopped to release the camera
2. `libcamera-still` captures a 640x480 image
3. The image is loaded, cropped to square, resized to 150x150
4. Saved as PNG to the specified filename
5. Preview is restarted if it was active

### Camera Detection
Camera availability is checked using `libcamera-hello --list-cameras` on initialization.

## Troubleshooting

### No Camera Detected
- Ensure the camera ribbon cable is properly connected
- Check that the camera is enabled in `raspi-config`
- Run `libcamera-hello --list-cameras` to verify camera is visible

### Preview Not Showing
- Check logs for "CAMERA: Preview started" message
- Verify libcamera-vid is installed and working
- Try running `libcamera-vid -t 5000` manually

### Photos Not Saving
- Check that Resources/Photos directory is writable
- Verify sufficient disk space
- Check logs for capture error messages
