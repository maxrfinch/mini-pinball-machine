# Button LED Effects Guide

This document provides detailed information about the button LED effect system for the arcade controller.

## Overview

The firmware includes a comprehensive button LED effect system with 9 different modes that can be triggered via serial commands or by game logic. Each effect is designed for specific game states and provides visual feedback to the player.

## Effect Modes

### 1. OFF (BTN_EFFECT_OFF)
- **Command**: `CMD BUTTON_EFFECT OFF`
- **Description**: All button LEDs turned off
- **Use Case**: When LEDs should be disabled

### 2. Ready Steady Glow (BTN_EFFECT_READY_STEADY_GLOW)
- **Command**: `CMD BUTTON_EFFECT READY_STEADY_GLOW`
- **Description**: Idle/attract mode breathing effect
- **Pattern**:
  - Left button: Slow breathing fade (0→180→0), ~2.5 Hz
  - Right button: Same as left but offset by 1 second
  - Center button: Steady glow at 200 brightness
- **Use Case**: Machine idle, attract mode, menu idle
- **Duration**: Continuous loop

### 3. Flipper Feedback (BTN_EFFECT_FLIPPER_FEEDBACK)
- **Command**: `CMD BUTTON_EFFECT FLIPPER_FEEDBACK`
- **Description**: Quick visual confirmation on button press
- **Pattern**:
  - Left/Right buttons: Fast pop to max (255), immediate drop to 0, then return to previous state
  - Duration: ~100ms total
  - Center button: Steady at 200
- **Use Case**: Visual feedback for left/right flipper presses
- **Duration**: One-shot effect, automatically returns to READY_STEADY_GLOW after completion

### 4. Center Hit Pulse (BTN_EFFECT_CENTER_HIT_PULSE)
- **Command**: `CMD BUTTON_EFFECT CENTER_HIT_PULSE`
- **Description**: Urgent strobe for ball launch
- **Pattern**:
  - Center button: Rapid double-strobe (255→0→255→0), then 300ms dark
  - Cycle: 40ms on, 40ms off, 40ms on, 300ms off
  - Left/Right buttons: Off
- **Use Case**: Center button when ball is ready to launch
- **Duration**: Continuous loop until changed

### 5. Skill Shot Build-Up (BTN_EFFECT_SKILL_SHOT_BUILDUP)
- **Command**: `CMD BUTTON_EFFECT SKILL_SHOT_BUILDUP`
- **Description**: Tension-building ramp before launch
- **Pattern**:
  - All buttons: Linear ramp from 0→255 over 2 seconds, then snap to 0
- **Use Case**: Before launching ball, skill shot mode
- **Duration**: Continuous loop

### 6. Ball Saved (BTN_EFFECT_BALL_SAVED)
- **Command**: `CMD BUTTON_EFFECT BALL_SAVED`
- **Description**: Alternating flash for multiball/save mode
- **Pattern**:
  - Left/Right buttons: Alternating flashes with decaying speed
  - 8 cycles over ~2 seconds
  - Each cycle gets progressively slower
  - Center button: Off
- **Use Case**: Temporary multiball or ball save mode
- **Duration**: One-shot effect, automatically returns to READY_STEADY_GLOW after completion

### 7. Powerup/Multiball Alert (BTN_EFFECT_POWERUP_ALERT)
- **Command**: `CMD BUTTON_EFFECT POWERUP_ALERT`
- **Description**: Chaotic strobe for special moments
- **Pattern**:
  - All buttons: Randomized on/off states at high frequency
  - Pseudo-random brightness (255 or 0)
- **Use Case**: Multiball start, jackpot, special game states
- **Duration**: 1.5 seconds, then automatically returns to READY_STEADY_GLOW

### 8. Extra Ball Award (BTN_EFFECT_EXTRA_BALL_AWARD)
- **Command**: `CMD BUTTON_EFFECT EXTRA_BALL_AWARD`
- **Description**: Celebratory pulses
- **Pattern**:
  - Center button: Three pulses (200ms each, 100ms gap)
  - Followed by: One long fade-out over 1 second
  - Left/Right buttons: Off
- **Use Case**: Extra ball award
- **Duration**: ~1.9 seconds total, then returns to READY_STEADY_GLOW

### 9. Game Over Fade (BTN_EFFECT_GAME_OVER_FADE)
- **Command**: `CMD BUTTON_EFFECT GAME_OVER_FADE`
- **Description**: Slow cooldown effect
- **Pattern**:
  - All buttons: Extremely slow triangle wave (bright→off→bright, 2.5s cycle)
- **Use Case**: Game over state
- **Duration**: Continuous loop

### 10. Menu Navigation (BTN_EFFECT_MENU_NAVIGATION)
- **Command**: `CMD BUTTON_EFFECT MENU_NAVIGATION`
- **Description**: Selection indicators for menu mode
- **Pattern**:
  - Selected button: 255 brightness
  - Other buttons: 80 brightness
- **Use Case**: Menu navigation
- **Duration**: Continuous until changed
- **Note**: Use `buttons_set_menu_selection(button)` in code to change selection

## Usage Examples

### Via Serial Commands

Connect to the KB2040 via USB serial and send commands:

```bash
# Screen
screen /dev/ttyACM0

# Or minicom
minicom -D /dev/ttyACM0
```

Example command sequence:
```
CMD BUTTON_EFFECT READY_STEADY_GLOW
# Wait 5 seconds
CMD BUTTON_EFFECT CENTER_HIT_PULSE
# Wait 3 seconds
CMD BUTTON_EFFECT POWERUP_ALERT
# Automatically returns to READY_STEADY_GLOW after 1.5s
```

### In Game Code

```c
// Start attract mode
buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);

// When ball is ready to launch
buttons_start_effect(BTN_EFFECT_CENTER_HIT_PULSE);

// On multiball activation
buttons_start_effect(BTN_EFFECT_POWERUP_ALERT);

// On game over
buttons_start_effect(BTN_EFFECT_GAME_OVER_FADE);

// In menu mode
buttons_start_effect(BTN_EFFECT_MENU_NAVIGATION);
buttons_set_menu_selection(BUTTON_CENTER);  // Highlight center button
```

## Integration with Game States

### Recommended Effect Mapping

| Game State | Button Effect | Notes |
|------------|---------------|-------|
| Attract/Idle | READY_STEADY_GLOW | Draws attention, looks alive |
| Menu | MENU_NAVIGATION | Clear visual selection |
| Ball Ready | CENTER_HIT_PULSE | Urgent, indicates action needed |
| Skill Shot | SKILL_SHOT_BUILDUP | Builds tension |
| Normal Play | READY_STEADY_GLOW | Keep subtle feedback |
| Flipper Press | FLIPPER_FEEDBACK | Quick one-shot confirmation |
| Ball Save Active | BALL_SAVED | Time-limited mode indicator |
| Multiball Start | POWERUP_ALERT | Excitement and chaos |
| Extra Ball | EXTRA_BALL_AWARD | Celebratory moment |
| Game Over | GAME_OVER_FADE | Wind down, cooldown |

### Auto-Transitioning Effects

Some effects automatically transition back to READY_STEADY_GLOW:
- FLIPPER_FEEDBACK (after 100ms)
- BALL_SAVED (after ~2 seconds)
- POWERUP_ALERT (after 1.5 seconds)
- EXTRA_BALL_AWARD (after ~1.9 seconds)

This ensures the system always has a sensible default state.

## Technical Details

### Update Rate
- Button LEDs are updated every 10ms from the main loop via `buttons_update_leds()`
- Effect timing uses millisecond precision
- Frame counter provides pseudo-random values for chaotic effects

### Hardware
- Button LEDs are controlled via Arcade Seesaw board (Adafruit 5296)
- I²C address: 0x3A
- PWM resolution: 16-bit (0-65535)
- Brightness range: 0-255 (mapped to PWM internally)

### Performance
- Effects use integer math and lookup tables where possible
- Sine waves computed using standard library `sinf()`
- No blocking delays; all effects are time-based state machines

## Debugging

### Test Individual Effects

```
CMD BUTTON_EFFECT OFF
# Verify all LEDs turn off

CMD BUTTON_EFFECT READY_STEADY_GLOW
# Watch left and right breathe with offset, center steady

CMD BUTTON_EFFECT SKILL_SHOT_BUILDUP
# Watch all ramp up over 2 seconds repeatedly

CMD BUTTON_EFFECT POWERUP_ALERT
# Chaotic strobe for 1.5s, then returns to glow
```

### Monitor Serial Output

The firmware logs button presses and effect changes:
```
[APP] MODE set to 0
BUTTON LEFT: PRESSED
BUTTON LEFT: RELEASED
```

### Debug Mode
After 30 seconds of inactivity, firmware enters debug mode with different button LED behavior:
- Left: Slow pulse
- Center: Steady on
- Right: Fast pulse

Send any command to exit debug mode and return to normal operation.

## Customization

To modify effect parameters, edit the `buttons_update_leds()` function in `src/buttons.c`:

- Timing: Adjust period_ms values
- Brightness: Modify max brightness values
- Patterns: Change mathematical curves or state machines
- Transitions: Modify auto-return behavior

Example: Change READY_STEADY_GLOW breathing speed:
```c
// Faster breathing (was 400ms, now 300ms)
uint32_t period_ms = 300; // ~3.3 Hz instead of 2.5 Hz
```

## Future Enhancements

Possible additions to the effect system:
- Configurable effect parameters via commands
- Effect chaining and playlists
- Brightness scaling multiplier
- Per-button independent effects
- Save last effect state to EEPROM
- Smooth transitions between effects

## See Also

- [firmware/README.md](README.md) - Main firmware documentation
- [firmware/HARDWARE_SETUP.md](HARDWARE_SETUP.md) - Hardware wiring guide
- [firmware/DIAGNOSTICS.md](DIAGNOSTICS.md) - Troubleshooting guide
