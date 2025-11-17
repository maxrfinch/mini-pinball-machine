#ifndef PHYSICS_H
#define PHYSICS_H

#include "gameStruct.h"

// Initialize physics system (cpSpace, walls, bumpers, flippers, collision handlers)
// Returns pointers to bumpers array, left flipper body, and right flipper body via out parameters
void physics_init(GameStruct *game, Bumper **out_bumpers, cpBody **out_leftFlipperBody, cpBody **out_rightFlipperBody);

// Step the physics simulation forward by dt seconds
void physics_step(GameStruct *game, float dt);

// Clean up physics resources
void physics_shutdown(GameStruct *game);

// Add a ball to the physics simulation
void physics_add_ball(GameStruct *game, float px, float py, float vx, float vy, int type);

#endif // PHYSICS_H
