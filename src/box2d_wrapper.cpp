/*
 * box2d_wrapper.cpp
 *
 * Implementation of C-compatible wrapper functions for Box2D
 */

#include <box2d/box2d.h>

extern "C" {
#include "box2d_wrapper.h"
}

// Body accessor functions
extern "C" Vec2 b2Body_GetPosition(b2Body* body) {
    b2Vec2 pos = body->GetPosition();
    Vec2 result = {pos.x, pos.y};
    return result;
}

extern "C" Vec2 b2Body_GetLinearVelocity(b2Body* body) {
    b2Vec2 vel = body->GetLinearVelocity();
    Vec2 result = {vel.x, vel.y};
    return result;
}

extern "C" float b2Body_GetAngle(b2Body* body) {
    return body->GetAngle();
}

extern "C" float b2Body_GetAngularVelocity(b2Body* body) {
    return body->GetAngularVelocity();
}

// Body setter functions
extern "C" void b2Body_SetTransform(b2Body* body, Vec2 position, float angle) {
    body->SetTransform(b2Vec2(position.x, position.y), angle);
}

extern "C" void b2Body_SetLinearVelocity(b2Body* body, Vec2 velocity) {
    body->SetLinearVelocity(b2Vec2(velocity.x, velocity.y));
}

extern "C" void b2Body_SetAngularVelocity(b2Body* body, float omega) {
    body->SetAngularVelocity(omega);
}

extern "C" void b2Body_ApplyForceToCenter(b2Body* body, Vec2 force) {
    body->ApplyForceToCenter(b2Vec2(force.x, force.y), true);
}

// World functions
extern "C" void b2World_DestroyBody(b2World* world, b2Body* body) {
    world->DestroyBody(body);
}
