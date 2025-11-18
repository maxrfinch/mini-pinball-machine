#ifndef PHYSICS_H
#define PHYSICS_H

#include "gameStruct.h"

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

// Animation state for lower bumpers (set by collision handlers, read by rendering code)
extern float leftLowerBumperAnim;
extern float rightLowerBumperAnim;

#endif // PHYSICS_H
