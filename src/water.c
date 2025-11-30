#include "water.h"
#include "raylib.h"
#include <math.h>

void Water_Init(WaterSystem *ws) {
    for (int i = 0; i < RIPPLE_SAMPLES; i++) {
        ws->rippleHeight[i] = 0.0f;
        ws->rippleVelocity[i] = 0.0f;
    }
    ws->impactIntensity = 0.0f;
}

void Water_AddImpulse(WaterSystem *ws, float xWorld, float impulse) {
    int idx = (int)((xWorld - WATER_LEFT) / WATER_WIDTH * RIPPLE_SAMPLES);
    TraceLog(LOG_DEBUG, "[WATER] Water_AddImpulse: xWorld=%.2f, impulse=%.4f, idx=%d", xWorld, impulse, idx);
    if (idx >= 0 && idx < RIPPLE_SAMPLES) {
        ws->rippleVelocity[idx] += impulse;
        TraceLog(LOG_DEBUG, "[WATER] Water_AddImpulse: velocity[%d] updated to %.4f", idx, ws->rippleVelocity[idx]);
    } else {
        TraceLog(LOG_DEBUG, "[WATER] Water_AddImpulse: idx %d out of bounds [0, %d)", idx, RIPPLE_SAMPLES);
    }
}

void Water_Update(WaterSystem *ws, Resources *res, float dt) {
    // Decay water impact intensity
    ws->impactIntensity *= 0.95f;
    if (ws->impactIntensity < 0.0f) ws->impactIntensity = 0.0f;
    if (ws->impactIntensity > 1.0f) ws->impactIntensity = 1.0f;

    float t = GetTime();
    
    // Add idle wave motion
    for (int i = 0; i < RIPPLE_SAMPLES; i++) {
        ws->rippleHeight[i] += 0.002f * sinf(t * 1.5f + i * 0.15f);
    }

    // Neighbor propagation (spring-like behavior)
    float spread = 0.25f;
    for (int i = 1; i < RIPPLE_SAMPLES - 1; i++) {
        ws->rippleVelocity[i] += spread * (ws->rippleHeight[i-1] + ws->rippleHeight[i+1] - 2.0f * ws->rippleHeight[i]);
    }

    // Apply damping and update positions
    for (int i = 0; i < RIPPLE_SAMPLES; i++) {
        ws->rippleVelocity[i] *= 0.985f;
        ws->rippleHeight[i] += ws->rippleVelocity[i];
    }

    // Update ripple texture
    unsigned char rippleData[RIPPLE_SAMPLES * 4];
    for (int i = 0; i < RIPPLE_SAMPLES; i++) {
        // Convert float height to grayscale byte (centered at 128)
        int val = (int)(128.0f + ws->rippleHeight[i] * 50.0f);
        if (val < 0) val = 0;
        if (val > 255) val = 255;
        rippleData[i * 4 + 0] = (unsigned char)val;
        rippleData[i * 4 + 1] = (unsigned char)val;
        rippleData[i * 4 + 2] = (unsigned char)val;
        rippleData[i * 4 + 3] = 255;
    }

    // Log ripple height, velocity, and texture data periodically (every ~1 second)
    static float lastLogTime = 0.0f;
    if (t - lastLogTime > 1.0f) {
        int midIdx = RIPPLE_SAMPLES / 2;
        int lastIdx = RIPPLE_SAMPLES - 1;
        TraceLog(LOG_DEBUG, "[WATER] Water_Update: rippleHeight sample values: [0]=%.4f, [%d]=%.4f, [%d]=%.4f",
                 ws->rippleHeight[0], midIdx, ws->rippleHeight[midIdx], lastIdx, ws->rippleHeight[lastIdx]);
        TraceLog(LOG_DEBUG, "[WATER] Water_Update: rippleVelocity sample values: [0]=%.4f, [%d]=%.4f, [%d]=%.4f",
                 ws->rippleVelocity[0], midIdx, ws->rippleVelocity[midIdx], lastIdx, ws->rippleVelocity[lastIdx]);
        TraceLog(LOG_DEBUG, "[WATER] Water_Update: impactIntensity=%.4f", ws->impactIntensity);
        
        // Log ripple data sample values before texture update
        int midDataIdx = midIdx * 4;
        int lastDataIdx = lastIdx * 4;
        TraceLog(LOG_DEBUG, "[WATER] UpdateTexture: rippleData sample values: [0]=%d, [%d]=%d, [%d]=%d",
                 rippleData[0], midDataIdx, rippleData[midDataIdx], lastDataIdx, rippleData[lastDataIdx]);
        
        lastLogTime = t;
    }

    UpdateTexture(res->rippleTexture, rippleData);
}
