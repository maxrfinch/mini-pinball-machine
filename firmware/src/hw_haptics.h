#ifndef HW_HAPTICS_H
#define HW_HAPTICS_H

#include <stdint.h>

// Phase 3 - Haptic motor control
// TODO: Implement in Phase 3

typedef enum {
    HAPTIC_SIDE_LEFT = 0,
    HAPTIC_SIDE_RIGHT = 1
} HapticSide;

// Initialize haptic motor hardware
void hw_haptics_init(void);

// Trigger haptic feedback
// side: LEFT or RIGHT motor
// pattern: vibration pattern ID
// strength: 0-255 intensity
// duration: duration in milliseconds
void hw_haptics_trigger(HapticSide side, uint8_t pattern, uint8_t strength, uint16_t duration);

// Update haptic motors (call from main loop)
void hw_haptics_update(void);

#endif // HW_HAPTICS_H
