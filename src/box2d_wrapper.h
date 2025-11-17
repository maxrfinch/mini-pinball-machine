/*
 * box2d_wrapper.h
 *
 * C-compatible wrapper functions for Box2D C++ API
 * Provides simple accessors for common Box2D operations
 */

#ifndef BOX2D_WRAPPER_H
#define BOX2D_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
typedef struct b2Body b2Body;
typedef struct b2World b2World;

// Vector type for C compatibility
typedef struct {
    float x;
    float y;
} Vec2;

// Body accessor functions
Vec2 b2Body_GetPosition(b2Body* body);
Vec2 b2Body_GetLinearVelocity(b2Body* body);
float b2Body_GetAngle(b2Body* body);
float b2Body_GetAngularVelocity(b2Body* body);

// Body setter functions
void b2Body_SetTransform(b2Body* body, Vec2 position, float angle);
void b2Body_SetLinearVelocity(b2Body* body, Vec2 velocity);
void b2Body_SetAngularVelocity(b2Body* body, float omega);
void b2Body_ApplyForceToCenter(b2Body* body, Vec2 force);

// World functions
void b2World_DestroyBody(b2World* world, b2Body* body);

#ifdef __cplusplus
}
#endif

#endif // BOX2D_WRAPPER_H
