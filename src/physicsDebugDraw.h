#ifndef HEADER_PHYSICS_DEBUG_DRAW
#define HEADER_PHYSICS_DEBUG_DRAW

#include "raylib.h"

// Forward declarations for Box2D types
typedef struct b2World b2World;

// Simple color structure to replace cpSpaceDebugColor
typedef struct {
    float r, g, b, a;
} DebugColor;

// Debug draw functions for Box2D
void Box2DDebugDrawWorld(b2World* world);

#endif
