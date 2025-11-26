# Camera Implementation for Top-3 Game Over Scene

## Overview
This implementation adds camera functionality to the Top-3 Game Over scene, allowing players to capture a photo when they achieve a top-3 score.

## Architecture

### Platform Support
- **Raspberry Pi**: Full camera support using Pi Camera Module 3 via libcamera C++ API
- **Other Platforms**: No-op stubs (camera disabled cleanly)

### Components

1. **camera.h**: C interface for camera operations
2. **camera_libcamera.hpp/cpp**: C++ wrapper around libcamera API (Pi only)
3. **camera_wrapper.cpp**: C-to-C++ bridge that exposes camera functionality to C code

### Key Features

- **Live Preview**: 150×150 pixel square preview box
  - Position: Centered horizontally, vertical center at Y=325
  - Square-cropped before scaling
  - Updates during name entry

- **Photo Capture**: 
  - Triggered on score submission (pressing enter on 5th character)
  - Filename format: `Resources/Photos/<username>_<score>.png`
  - Coordinated with NeoPixel flash effect

- **NeoPixel Integration**:
  - Preview mode: `CMD NEO EFFECT NONE` (LEDs off for photo)
  - Flash: `CMD NEO EFFECT CAMERA_FLASH` (board 3 white flash)
  - Restore: `CMD NEO EFFECT ATTRACT` (return to idle)

## Usage Flow

1. **Enter Top-3 Game Over Scene**
   - Camera initialized (if available)
   - Preview starts
   - NeoPixels set to NONE mode

2. **Name Entry**
   - Live preview displayed in 150×150 box
   - Player enters name using left/right/center buttons

3. **Submit Score**
   - Trigger NeoPixel flash
   - Wait 200ms for flash
   - Capture photo
   - Stop preview
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

## Current Implementation Status

### Working Features
- Camera initialization and detection
- Photo capture using libcamera-still command
- NeoPixel LED coordination
- Platform detection and graceful fallback
- Error handling

### Known Limitations
- **Live Preview**: Not yet implemented. The C++ libcamera API requires:
  - Event loop integration for request handling
  - Frame buffer mapping and memory access
  - Format conversion and texture updates
  - Currently shows placeholder text instead of live feed

- **Workaround**: Photo capture uses `libcamera-still` external command
  - Reliable and tested
  - Captures 150x150 square image directly
  - Should be replaced with native C++ API calls in future

### Future Improvements
- Complete libcamera C++ API integration for live preview
- Replace external command with native frame capture
- Add JPEG encoding for captured frames
- Optimize preview update rate
- Add zoom/crop configuration options
- Implement CAMERA_PREVIEW effect in firmware if needed
- Add proper event loop integration for async capture
