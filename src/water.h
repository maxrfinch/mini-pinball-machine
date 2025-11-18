#ifndef WATER_H
#define WATER_H

#include "constants.h"
#include "resources.h"

typedef struct {
    float rippleHeight[RIPPLE_SAMPLES];
    float rippleVelocity[RIPPLE_SAMPLES];
    float impactIntensity;
} WaterSystem;

// Initialize water system
void Water_Init(WaterSystem *ws);

// Add impulse to water at a specific x world coordinate
void Water_AddImpulse(WaterSystem *ws, float xWorld, float impulse);

// Update water simulation and shader parameters
void Water_Update(WaterSystem *ws, Resources *res, float dt);

#endif // WATER_H
