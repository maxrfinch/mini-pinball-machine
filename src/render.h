#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"
#include "resources.h"
#include "gameStruct.h"
#include "physics.h"

#ifdef __cplusplus
extern "C" {
#endif

// Draws the entire game world for an active gameplay state
// Includes: background, bumpers, balls, flippers, effects
void Render_Gameplay(const GameStruct *game, const Resources *res, 
                     const Bumper *bumpers, int numBumpers,
                     b2BodyId leftFlipperBody, b2BodyId rightFlipperBody,
                     float shaderSeconds, float iceOverlayAlpha,
                     int debugDrawEnabled, long long elapsedTimeStart);

#ifdef __cplusplus
}
#endif

#endif // RENDER_H
