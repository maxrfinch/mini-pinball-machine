#ifndef POWERUPS_H
#define POWERUPS_H

#include "gameStruct.h"
#include "inputManager.h"
#include "soundManager.h"

typedef struct {
    float slowMotionFactor;
    float iceOverlayAlpha;
    float multiballOverlayY;
} PowerupSystem;

// Initialize powerup system
void Powerups_Init(GameStruct *game, PowerupSystem *ps);

// Update powerup system (slow-mo, overlays, cooldowns)
void Powerups_Update(GameStruct *game,
                     PowerupSystem *ps,
                     InputManager *input,
                     SoundManager *sound,
                     float effectiveTimestep);

// Check and handle multiball powerup activation and reset
// Called when ball powerup is ready or in progress
void Powerups_CheckMultiball(GameStruct *game, Bumper *bumpers, SoundManager *sound, InputManager *input);

// Check and handle bumper powerup activation and reset
// Called when bumper powerup is ready or in progress
void Powerups_CheckBumperPowerup(GameStruct *game, Bumper *bumpers, SoundManager *sound);

#endif // POWERUPS_H
