/**
 * controller_state.h
 * 
 * Central controller state machine definitions
 */

#ifndef CONTROLLER_STATE_H
#define CONTROLLER_STATE_H

#include <stdint.h>
#include <stdbool.h>
#include "types.h"

// Controller substate - finer-grained state within a mode
typedef enum {
    SUB_NONE = 0,
    SUB_MENU_IDLE,
    SUB_MENU_NAV,
    SUB_BALL_READY,
    SUB_BALL_IN_PLAY,
    SUB_SKILL_SHOT,
    SUB_MULTIBALL
} controller_substate_t;

// Effect priority - determines which effect wins
typedef enum {
    PRIORITY_BASE = 0,      // Long-lived defaults based on MODE/SUBSTATE
    PRIORITY_EVENT,         // Short-lived game events (jackpot, ball save, etc.)
    PRIORITY_CRITICAL       // Manual overrides, debug, errors
} effect_priority_t;

// Controller state structure
typedef struct {
    GameMode               mode;
    controller_substate_t  substate;

    // Menu state
    uint8_t menu_index;
    uint8_t menu_count;
    bool    menu_smart;   // true = controller moves index; false = host owns index

    // Game flags
    bool    ball_ready;
    bool    skill_shot_active;
    bool    multiball_active;

    // Current priorities for subsystems
    effect_priority_t np_prio;   // NeoPixel
    effect_priority_t btn_prio;  // Button LEDs
} controller_state_t;

// Initialize controller state
void controller_state_init(void);

// Get current state (read-only access)
const controller_state_t* controller_get_state(void);

// Set mode and apply base profile
void controller_set_mode(GameMode mode);

// Set game state flags
void controller_set_ball_ready(bool ready);
void controller_set_skill_shot(bool active);
void controller_set_multiball(bool active);

// Menu configuration
void controller_set_menu_mode(bool smart);
void controller_set_menu_size(uint8_t count);
void controller_set_menu_index(uint8_t index);

// Apply base profile based on current mode/state
void controller_apply_base_profile(void);

// Handle button press in current mode (returns true if handled)
bool controller_handle_button_press(Button button);

// Priority-based effect setters
void controller_neopixel_set_effect(LedEffect effect, effect_priority_t priority);
void controller_button_set_effect(ButtonLEDEffect effect, effect_priority_t priority);

// One-shot event effects with timeout
void controller_neopixel_play_one_shot(LedEffect effect, effect_priority_t priority, uint32_t duration_ms);
void controller_button_play_one_shot(ButtonLEDEffect effect, effect_priority_t priority, uint32_t duration_ms);

// Check and handle event timeouts (call from main loop)
void controller_check_event_timeouts(void);

// Manual override functions
void controller_neopixel_override(LedEffect effect);
void controller_neopixel_clear_override(void);
void controller_button_override(ButtonLEDEffect effect);
void controller_button_clear_override(void);

#endif // CONTROLLER_STATE_H
