#ifndef FLIPPERS_H
#define FLIPPERS_H

#include "gameStruct.h"
#include "inputManager.h"
#include "soundManager.h"
#include <box2d/box2d.h>

// Initialize flipper system
void Flippers_Init(GameStruct *game, b2BodyId *leftFlipperBody, b2BodyId *rightFlipperBody);

// Update flipper angles and sounds based on input
// Returns the delta angular velocities for left and right flippers (for physics integration)
void Flippers_Update(GameStruct *game,
                     b2BodyId *leftFlipperBody,
                     b2BodyId *rightFlipperBody,
                     InputManager *input,
                     SoundManager *sound,
                     float dt,
                     float *out_leftDeltaAngularVelocity,
                     float *out_rightDeltaAngularVelocity);

#endif // FLIPPERS_H
