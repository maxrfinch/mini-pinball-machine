/**
 * controller_state.h
 * 
 * Controller effect management - Pi-centric architecture
 * Controller executes commands without maintaining game state
 */

#ifndef CONTROLLER_STATE_H
#define CONTROLLER_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"
#include "buttons.h"

// Effect priority - determines which effect wins when multiple requests conflict
typedef enum {
    PRIORITY_BASE = 0,      // Normal effects
    PRIORITY_EVENT,         // Temporary event effects (auto-revert after duration)
    PRIORITY_CRITICAL       // Debug/manual overrides
} effect_priority_t;

// Controller state structure (minimal - just for effect priority tracking)
typedef struct {
    // Current priorities for subsystems
    effect_priority_t np_prio;   // NeoPixel
    effect_priority_t btn_prio;  // Button LEDs
} controller_state_t;

// Initialize controller
void controller_state_init(void);

// Get current state (read-only access)
const controller_state_t* controller_get_state(void);

// Priority-based effect setters
void controller_neopixel_set_effect(LedEffect effect, effect_priority_t priority);
void controller_button_set_effect(ButtonLEDEffect effect, effect_priority_t priority);

// Per-button effect setters (allows different effects on different buttons)
void controller_button_set_effect_single(Button button, ButtonLEDEffect effect, effect_priority_t priority);
void controller_button_set_effect_all(ButtonLEDEffect effect, effect_priority_t priority);

// One-shot event effects with auto-timeout
void controller_neopixel_play_one_shot(LedEffect effect, effect_priority_t priority, uint32_t duration_ms);
void controller_button_play_one_shot(ButtonLEDEffect effect, effect_priority_t priority, uint32_t duration_ms);

// Check and handle event timeouts (call from main loop)
void controller_check_event_timeouts(void);

#endif // CONTROLLER_STATE_H
