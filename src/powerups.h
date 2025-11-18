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

#endif // POWERUPS_H
