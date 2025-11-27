# Power-Up Interaction System

This document describes the interaction rules between the Water and Slow-Mo power-ups in the Mini Pinball Machine game.

## Overview

The game features two primary power-ups that have special interaction rules:
- **Water Power-up**: Raises the water level, providing buoyancy effects
- **Slow-Mo Power-up**: Slows down game time for strategic play

## Interaction Rules

### Rule 1: Mutual Exclusivity - Water Blocks Slow-Mo
If the player activates the Water power-up, any active Slow-Mo effects are deactivated, and Slow-Mo cannot be activated during the Water power-up.

**Implementation:**
- When Water activates (`powerupScoreDisplay >= powerupTargetScore`), the system checks if Slow-Mo is active and prevents Water activation
- When hitting the Slow-Mo bumper, the system checks if Water is active (`waterPowerupState > 0`) and prevents Slow-Mo activation

### Rule 2: Mutual Exclusivity - Slow-Mo Blocks Water
If the player activates Slow-Mo, the Water power-up will not activate.

**Implementation:**
- Water power-up activation in `game.c` now checks `slowMotion == 0` before triggering

### Rule 3: Bonus Points During Water
If the Slow-Mo bumper is hit during an active Water power-up, award 1000 bonus points to the player.

**Implementation:**
- In `physics.c`, when the Slow-Mo bumper (type `BUMPER_TYPE_SLOW_MOTION`) is hit while `waterPowerupState > 0`, the player receives 1000 points instead of the Slow-Mo effect

### Rule 4: Pause Point Accumulation During Slow-Mo
While Slow-Mo is active, pause any point accumulation towards the Water power-up meter (powerupScore).

**Implementation:**
- All scoring locations that contribute to `powerupScore` now additionally check `slowMotion == 0` before adding points
- This affects: standard bumpers, lane targets, water bumpers, slingshots, multiball, and bumper powerups

## Technical Details

### Files Modified
- `src/game.c` - Water power-up activation with Slow-Mo check
- `src/physics.c` - Collision handlers for bumpers and slingshots
- `src/powerups.c` - Multiball and bumper power-up scoring

### State Variables
- `game->slowMotion` - 1 if Slow-Mo is active, 0 otherwise
- `game->waterPowerupState` - 0 if inactive, 1 if rising, 2 if draining
- `game->powerupScore` - Accumulates points towards Water power-up activation

## Testing Scenarios

1. **Water disables Slow-Mo activation**: With Water active, hitting the Slow-Mo bumper awards 1000 points but doesn't activate Slow-Mo
2. **Slow-Mo blocks Water**: With Slow-Mo active, reaching the powerup target score doesn't activate Water
3. **Bonus points work**: 1000 points awarded when hitting Slow-Mo bumper during Water
4. **Point accumulation paused**: During Slow-Mo, powerupScore doesn't increase from any scoring event
