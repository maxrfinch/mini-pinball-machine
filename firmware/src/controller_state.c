/**
 * controller_state.c
 * 
 * Controller effect management - Pi-centric architecture
 * Controller executes commands without maintaining game state
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "controller_state.h"
#include "neopixel.h"
#include "buttons.h"

// Global controller state (minimal - just priority tracking)
static controller_state_t g_state;

// Event timeout tracking
static uint32_t np_event_deadline_ms = 0;
static uint32_t btn_event_deadline_ms = 0;

// Constants
#define NUM_BUTTONS 3  // Number of physical buttons (LEFT, CENTER, RIGHT)

void controller_state_init(void) {
    memset(&g_state, 0, sizeof(g_state));
    
    // Default priority
    g_state.np_prio = PRIORITY_BASE;
    g_state.btn_prio = PRIORITY_BASE;
    
    printf("[CONTROLLER] Initialized (Pi-centric mode)\n");
}

const controller_state_t* controller_get_state(void) {
    return &g_state;
}

// Priority-based effect setters for NeoPixels
void controller_neopixel_set_effect(LedEffect effect, effect_priority_t priority) {
    if (priority < g_state.np_prio) {
        printf("[CONTROLLER] NeoPixel effect ignored (priority %d < %d)\n", 
               priority, g_state.np_prio);
        return; // Ignore lower-priority request
    }

    g_state.np_prio = priority;
    neopixel_start_effect(effect);
    printf("[CONTROLLER] NeoPixel effect %d set at priority %d\n", effect, priority);
}

// Priority-based effect setter for all buttons
void controller_button_set_effect(ButtonLEDEffect effect, effect_priority_t priority) {
    if (priority < g_state.btn_prio) {
        printf("[CONTROLLER] Button effect ignored (priority %d < %d)\n", 
               priority, g_state.btn_prio);
        return; // Ignore lower-priority request
    }

    g_state.btn_prio = priority;
    buttons_start_effect(effect);
    printf("[CONTROLLER] Button effect %d set at priority %d\n", effect, priority);
}

// Set effect on all buttons (convenience wrapper)
void controller_button_set_effect_all(ButtonLEDEffect effect, effect_priority_t priority) {
    controller_button_set_effect(effect, priority);
}

// Set effect on a single button
// NOTE: Current implementation limitation - applies to all buttons
// The button parameter is accepted for API consistency but not yet used
// TODO: Enhance buttons.c to support true per-button effect control
void controller_button_set_effect_single(Button button, ButtonLEDEffect effect, effect_priority_t priority) {
    if (priority < g_state.btn_prio) {
        printf("[CONTROLLER] Button effect ignored (priority %d < %d)\n", 
               priority, g_state.btn_prio);
        return; // Ignore lower-priority request
    }

    g_state.btn_prio = priority;
    // Current limitation: applies effect to all buttons
    buttons_start_effect(effect);
    printf("[CONTROLLER] Button %d effect %d set at priority %d (applies to all)\n", button, effect, priority);
}

// One-shot event effects with timeout
void controller_neopixel_play_one_shot(LedEffect effect, effect_priority_t priority, uint32_t duration_ms) {
    controller_neopixel_set_effect(effect, priority);
    
    // Set deadline for returning to base priority
    np_event_deadline_ms = to_ms_since_boot(get_absolute_time()) + duration_ms;
}

void controller_button_play_one_shot(ButtonLEDEffect effect, effect_priority_t priority, uint32_t duration_ms) {
    controller_button_set_effect(effect, priority);
    
    // Set deadline for returning to base priority
    btn_event_deadline_ms = to_ms_since_boot(get_absolute_time()) + duration_ms;
}

// Check and handle event timeouts (call from main loop)
void controller_check_event_timeouts(void) {
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    
    // Check NeoPixel event timeout
    if (g_state.np_prio == PRIORITY_EVENT && np_event_deadline_ms > 0) {
        if (now_ms >= np_event_deadline_ms) {
            printf("[CONTROLLER] NeoPixel event timeout, returning to BASE\n");
            g_state.np_prio = PRIORITY_BASE;
            np_event_deadline_ms = 0;
            // Effect stays as-is; Pi needs to send new command to change it
        }
    }
    
    // Check button event timeout
    if (g_state.btn_prio == PRIORITY_EVENT && btn_event_deadline_ms > 0) {
        if (now_ms >= btn_event_deadline_ms) {
            printf("[CONTROLLER] Button event timeout, returning to BASE\n");
            g_state.btn_prio = PRIORITY_BASE;
            btn_event_deadline_ms = 0;
            // Effect stays as-is; Pi needs to send new command to change it
        }
    }
}
