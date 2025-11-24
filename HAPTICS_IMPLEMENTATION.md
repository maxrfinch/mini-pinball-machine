# Haptics System Implementation

This document describes the haptics system added to the mini-pinball-machine audio system.

## Overview

The audio system now supports a dedicated haptics channel for tactile feedback via a TT25-8 shaker, while maintaining all existing game audio functionality.

## Architecture

### Split-Channel Design

The system uses a **stereo output with split channel routing**:

- **LEFT CHANNEL** = Haptics only (TT25-8 shaker)
- **RIGHT CHANNEL** = Game audio only (speakers)

This is achieved using **raylib's native panning** capabilities:

1. All game audio (Music + Sound) is panned fully right using:
   - `SetMusicPan(music, 1.0f)`
   - `SetSoundPan(sound, 1.0f)`

2. Haptics are delivered via a dedicated AudioStream panned fully left:
   - `SetAudioStreamPan(hapticsStream, -1.0f)`

3. Raylib's internal mixer combines everything and outputs to the audio device

### Key Implementation Details

- **No custom mixer required** - raylib handles all mixing internally
- **Static buffer** for haptics (no per-frame allocation)
- **Procedural waveform generation** - all haptic effects are generated algorithmically
- **Non-blocking triggers** - haptic functions can be called from game logic at any time

## Haptic Effects

All effects are tuned for a TT25-8 shaker in a small, rigid acrylic enclosure (~3L).

### Effect Parameters

| Effect | Frequency | Duration | Amplitude | Waveform |
|--------|-----------|----------|-----------|----------|
| Flipper (empty) | 60 Hz | 12.5 ms | 0.2 | Sine |
| Flipper (hit) | 60 Hz | 20 ms | 0.4 | Square |
| Ball launch | 40→60 Hz | 90 ms | 0.4 (ramped) | Sine sweep |
| Light bumper | 55 Hz | 22.5 ms | 0.27 | Sine |
| Solid bumper | 50 Hz | 46 ms | 0.45 + 0.25 | Square (dual pulse) |
| Excitement buzz | 40→80 Hz | 210 ms | 0.45→0.35→0.25 | Sine sweep (3 segments) |

### Amplitude Tiers

Effects use tiered amplitudes to create distinct haptic sensations:

- **Very soft** (0.15-0.25): Subtle confirmation (empty flipper)
- **Soft/medium** (0.25-0.35): Light impacts (glancing hits)
- **Medium/strong** (0.35-0.50): Solid impacts (flipper+ball, strong bumpers)
- **Special event** (0.40-0.70): Big moments (multiball, jackpot)

Maximum amplitude is kept below 1.0 to protect the enclosure and maintain comfortable feedback.

## API

### Existing Functions (Unchanged)

All existing sound functions continue to work as before:

```c
SoundManager *initSound();
void updateSound(SoundManager *soundManager, GameStruct *game);
void shutdownSound(SoundManager *soundManager);

void playSlowdownSound(SoundManager *sound);
void playSpeedupSound(SoundManager *sound);
void playRedPowerupSound(SoundManager *sound);
void playBluePowerupSound(SoundManager *sound);
void playUpperBouncerSound(SoundManager *sound);
void playClick(SoundManager *sound);
void playBounce(SoundManager *sound);
void playBounce2(SoundManager *sound);
void playLaunch(SoundManager *sound);
void playFlipper(SoundManager *sound);
void playWater(SoundManager *sound);
void playWaterSplash(SoundManager *sound);
```

### New Haptic Functions

Six new functions trigger haptic effects:

```c
void sound_play_haptic_flipper_empty(SoundManager *sound);  // Subtle confirmation
void sound_play_haptic_flipper_hit(SoundManager *sound);     // Sharp impact
void sound_play_haptic_launch(SoundManager *sound);          // Rising energy
void sound_play_haptic_bumper_light(SoundManager *sound);    // Gentle tap
void sound_play_haptic_bumper_solid(SoundManager *sound);    // Strong thwack
void sound_play_haptic_excitement(SoundManager *sound);      // Big event buzz
```

**Note**: The `SoundManager` parameter is unused in haptic functions (haptics use global state) but is kept for API consistency with other sound functions.

## Usage Examples

### Basic Flipper Feedback

```c
// When flipper button is pressed but doesn't hit ball
sound_play_haptic_flipper_empty(game->sound);
playFlipper(game->sound);  // Audio feedback

// When flipper strikes ball
sound_play_haptic_flipper_hit(game->sound);
playFlipper(game->sound);  // Audio feedback
```

### Ball Launch

```c
// When ball is launched into play
sound_play_haptic_launch(game->sound);
playLaunch(game->sound);  // Audio feedback
```

### Bumper Hits

```c
// Light/glancing contact
sound_play_haptic_bumper_light(game->sound);
playBounce(game->sound);

// Solid impact
sound_play_haptic_bumper_solid(game->sound);
playBounce2(game->sound);
```

### Big Events

```c
// Multiball start, super jackpot, etc.
sound_play_haptic_excitement(game->sound);
playRedPowerupSound(game->sound);
```

## Technical Details

### Haptics Engine

The haptics engine is a simple state machine that:

1. Maintains one active effect at a time
2. Tracks elapsed time and oscillator phase
3. Generates one sample per AudioStream frame
4. Returns 0.0 when no effect is active

### Sample Generation

The `updateSound()` function:

1. Updates music streams (as before)
2. Checks if haptics stream needs data via `IsAudioStreamProcessed()`
3. Fills buffer with haptic samples (left channel) and silence (right channel)
4. Submits buffer via `UpdateAudioStream()`

### Memory Management

- `SoundManager` struct: allocated once in `initSound()`
- Audio buffer: static allocation (no per-frame malloc/free)
- Sound arrays: allocated once, freed in `shutdownSound()`

### Error Handling

- NULL checks for all malloc failures
- Safe cleanup on initialization failure
- NULL-safe shutdown function

## Hardware Requirements

- **Stereo audio output** (3.5mm jack or similar)
- **Left channel** wired to TT25-8 shaker (8Ω, 5W)
- **Right channel** wired to speakers (e.g., Adafruit 1669)
- **Audio amplifier** appropriate for both shaker and speakers

## Tuning

To adjust haptic feel, modify parameters in `soundManager.c`:

- **Frequency**: Change `freq` constants (40-80 Hz range recommended)
- **Duration**: Adjust duration constants (stay under ~250ms for crispness)
- **Amplitude**: Modify `amp` values (keep under 0.7 to protect enclosure)

The waveform generator is in `haptics_generate_sample()` and uses simple math:
- Sine waves: `sinf(phase)`
- Square waves: `sinf(phase) > 0 ? 1.0 : -1.0`
- Frequency sweeps: interpolate frequency over time

## Future Enhancements

Potential improvements:

1. **Effect queuing**: Support overlapping or queued effects
2. **Intensity scaling**: Global amplitude multiplier for user preference
3. **More effects**: Additional patterns for new game events
4. **Decay tails**: Gradual fade-out for some effects
5. **Dynamic tuning**: Adjust parameters based on game state

## Compatibility

The implementation uses standard raylib functions:
- `LoadAudioStream()` / `UpdateAudioStream()`
- `SetAudioStreamPan()` / `SetSoundPan()` / `SetMusicPan()`
- `PlaySound()` / `PlayMusicStream()`

These are available in raylib 3.0+. If using an older version, panning functions may need to be stubbed out (audio will play to both channels but functionality will otherwise work).
