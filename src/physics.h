#ifndef PHYSICS_H
#define PHYSICS_H

#include "gameStruct.h"
#include "inputManager.h"
#include "soundManager.h"

// Initialize physics system (Box2D world, walls, bumpers, flippers, collision handlers)
// Returns pointers to bumpers array, left flipper body, and right flipper body via out parameters
void physics_init(GameStruct *game, Bumper **out_bumpers, b2BodyId **out_leftFlipperBody, b2BodyId **out_rightFlipperBody);

// Step the physics simulation forward by dt seconds
void physics_step(GameStruct *game, float dt);

// Clean up physics resources
void physics_shutdown(GameStruct *game);

// Add a ball to the physics simulation
void physics_add_ball(GameStruct *game, float px, float py, float vx, float vy, int type);

// Draw physics debug visualization (walls, bodies, shapes)
void physics_debug_draw(GameStruct *game);

// Initialize flipper system
void physics_flippers_init(GameStruct *game, b2BodyId *leftFlipperBody, b2BodyId *rightFlipperBody);

// Update flipper angles and sounds based on input
// Returns the delta angular velocities for left and right flippers (for physics integration)
void physics_flippers_update(GameStruct *game,
                              b2BodyId *leftFlipperBody,
                              b2BodyId *rightFlipperBody,
                              InputManager *input,
                              SoundManager *sound,
                              float dt,
                              float *out_leftDeltaAngularVelocity,
                              float *out_rightDeltaAngularVelocity);

// Animation state for lower bumpers (set by collision handlers, read by rendering code)
extern float leftLowerBumperAnim;
extern float rightLowerBumperAnim;

#endif // PHYSICS_H
