/**
 * controller_state.c
 * 
 * Central controller state machine implementation
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "controller_state.h"
#include "neopixel.h"
#include "buttons.h"
#include "protocol.h"

// Global controller state
static controller_state_t g_state;

// Event timeout tracking
static uint32_t np_event_deadline_ms = 0;
static uint32_t btn_event_deadline_ms = 0;

// Constants
#define NUM_BUTTONS 3  // Number of physical buttons (LEFT, CENTER, RIGHT)

// Forward declarations
static void apply_mode_effects(void);
static void send_menu_event(const char* event_type, uint8_t index);
static void send_ball_launch_event(void);

void controller_state_init(void) {
    memset(&g_state, 0, sizeof(g_state));
    
    // Default state
    g_state.mode = MODE_ATTRACT;
    g_state.substate = SUB_NONE;
    g_state.menu_index = 0;
    g_state.menu_count = 0;
    g_state.menu_smart = false;
    g_state.ball_ready = false;
    g_state.skill_shot_active = false;
    g_state.multiball_active = false;
    g_state.np_prio = PRIORITY_BASE;
    g_state.btn_prio = PRIORITY_BASE;
    
    printf("[CONTROLLER] State machine initialized\n");
}

const controller_state_t* controller_get_state(void) {
    return &g_state;
}

void controller_set_mode(GameMode mode) {
    g_state.mode = mode;
    
    // Reset substate when mode changes
    g_state.substate = SUB_NONE;
    
    // Set appropriate default substates
    if (mode == MODE_MENU) {
        g_state.substate = SUB_MENU_IDLE;
    } else if (mode == MODE_GAME && g_state.ball_ready) {
        g_state.substate = SUB_BALL_READY;
    } else if (mode == MODE_GAME) {
        g_state.substate = SUB_BALL_IN_PLAY;
    }
    
    printf("[CONTROLLER] Mode set to %d, substate %d\n", mode, g_state.substate);
    
    // Apply base profile for new mode
    controller_apply_base_profile();
}

void controller_set_ball_ready(bool ready) {
    g_state.ball_ready = ready;
    
    if (g_state.mode == MODE_GAME) {
        g_state.substate = ready ? SUB_BALL_READY : SUB_BALL_IN_PLAY;
        controller_apply_base_profile();
    }
    
    printf("[CONTROLLER] Ball ready: %d\n", ready);
}

void controller_set_skill_shot(bool active) {
    g_state.skill_shot_active = active;
    
    if (g_state.mode == MODE_GAME && active) {
        g_state.substate = SUB_SKILL_SHOT;
        controller_apply_base_profile();
    }
    
    printf("[CONTROLLER] Skill shot active: %d\n", active);
}

void controller_set_multiball(bool active) {
    g_state.multiball_active = active;
    
    if (g_state.mode == MODE_GAME && active) {
        g_state.substate = SUB_MULTIBALL;
        controller_apply_base_profile();
    }
    
    printf("[CONTROLLER] Multiball active: %d\n", active);
}

void controller_set_menu_mode(bool smart) {
    g_state.menu_smart = smart;
    printf("[CONTROLLER] Menu mode: %s\n", smart ? "SMART" : "DUMB");
}

void controller_set_menu_size(uint8_t count) {
    g_state.menu_count = count;
    printf("[CONTROLLER] Menu size: %d\n", count);
}

void controller_set_menu_index(uint8_t index) {
    if (g_state.menu_count > 0 && index < g_state.menu_count) {
        g_state.menu_index = index;
        printf("[CONTROLLER] Menu index: %d\n", index);
        
        // Update button effect if in menu mode
        // Map menu index to button for visual feedback (0=LEFT, 1=CENTER, 2=RIGHT, wrap if >NUM_BUTTONS)
        if (g_state.mode == MODE_MENU) {
            Button button = (Button)(index % NUM_BUTTONS);
            buttons_set_menu_selection(button);
        }
    }
}

void controller_apply_base_profile(void) {
    // Only apply base profile if not overridden by higher priorities
    printf("[CONTROLLER] Applying base profile for mode %d, substate %d\n", 
           g_state.mode, g_state.substate);
    
    switch (g_state.mode) {
        case MODE_ATTRACT:
            // Only set if current priority is BASE
            if (g_state.np_prio == PRIORITY_BASE) {
                neopixel_start_effect(EFFECT_ATTRACT);
            }
            if (g_state.btn_prio == PRIORITY_BASE) {
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
            }
            break;

        case MODE_MENU:
            if (g_state.np_prio == PRIORITY_BASE) {
                neopixel_start_effect(EFFECT_ATTRACT);
            }
            if (g_state.btn_prio == PRIORITY_BASE) {
                buttons_start_effect(BTN_EFFECT_MENU_NAVIGATION);
                // Map menu index to button for visual feedback
                Button button = (Button)(g_state.menu_index % NUM_BUTTONS);
                buttons_set_menu_selection(button);
            }
            break;

        case MODE_GAME:
            if (g_state.skill_shot_active) {
                // Skill shot active - use buildup effect
                if (g_state.np_prio == PRIORITY_BASE) {
                    neopixel_start_effect(EFFECT_BALL_LAUNCH);
                }
                if (g_state.btn_prio == PRIORITY_BASE) {
                    buttons_start_effect(BTN_EFFECT_SKILL_SHOT_BUILDUP);
                }
            } else if (g_state.ball_ready) {
                if (g_state.np_prio == PRIORITY_BASE) {
                    neopixel_start_effect(EFFECT_BALL_LAUNCH);
                }
                if (g_state.btn_prio == PRIORITY_BASE) {
                    buttons_start_effect(BTN_EFFECT_CENTER_HIT_PULSE);
                }
            } else {
                if (g_state.np_prio == PRIORITY_BASE) {
                    neopixel_start_effect(EFFECT_NONE);
                }
                if (g_state.btn_prio == PRIORITY_BASE) {
                    buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
                }
            }
            break;

        case MODE_BALL_LOST:
            if (g_state.np_prio == PRIORITY_BASE) {
                neopixel_start_effect(EFFECT_RED_STROBE_5X);
            }
            if (g_state.btn_prio == PRIORITY_BASE) {
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
            }
            break;

        case MODE_HIGH_SCORE:
            if (g_state.np_prio == PRIORITY_BASE) {
                neopixel_start_effect(EFFECT_PINK_PULSE);
            }
            if (g_state.btn_prio == PRIORITY_BASE) {
                buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
            }
            break;

        case MODE_DEBUG:
        default:
            // Debug mode is handled separately
            break;
    }
}

// Helper to send menu events back to host
static void send_menu_event(const char* event_type, uint8_t index) {
    printf("EVT MENU_%s %d\n", event_type, index);
}

bool controller_handle_button_press(Button button) {
    // Handle button presses based on current mode
    switch (g_state.mode) {
        case MODE_MENU:
            if (g_state.menu_smart) {
                if (button == BUTTON_LEFT) {
                    // Move left with wrap-around
                    if (g_state.menu_index > 0) {
                        g_state.menu_index--;
                    } else if (g_state.menu_count > 0) {
                        g_state.menu_index = g_state.menu_count - 1;
                    }
                    // Update visual feedback
                    Button visual_button = (Button)(g_state.menu_index % NUM_BUTTONS);
                    buttons_set_menu_selection(visual_button);
                    send_menu_event("MOVE", g_state.menu_index);
                    return true;
                    
                } else if (button == BUTTON_RIGHT) {
                    // Move right with wrap-around
                    if (g_state.menu_count > 0) {
                        g_state.menu_index = (g_state.menu_index + 1) % g_state.menu_count;
                    }
                    // Update visual feedback
                    Button visual_button = (Button)(g_state.menu_index % NUM_BUTTONS);
                    buttons_set_menu_selection(visual_button);
                    send_menu_event("MOVE", g_state.menu_index);
                    return true;
                    
                } else if (button == BUTTON_CENTER) {
                    // Select current menu item
                    send_menu_event("SELECT", g_state.menu_index);
                    return true;
                }
            }
            break;

        case MODE_GAME:
            if (g_state.ball_ready && button == BUTTON_CENTER) {
                // Ball launch sequence
                g_state.ball_ready = false;
                g_state.substate = SUB_BALL_IN_PLAY;
                
                // Trigger launch animation
                controller_apply_base_profile();
                
                // Note: No EVT BALL_LAUNCH sent - host detects launch from raw button event
                return true;
            } else if (!g_state.ball_ready && (button == BUTTON_LEFT || button == BUTTON_RIGHT)) {
                // Flipper button pressed during gameplay - trigger feedback effect
                // Use one-shot with very short duration (100ms) so it auto-returns
                controller_button_play_one_shot(BTN_EFFECT_FLIPPER_FEEDBACK, PRIORITY_EVENT, 100);
                return true;
            }
            break;

        default:
            break;
    }
    
    return false; // Not handled
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

// Priority-based effect setters for buttons
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
    bool need_reapply = false;
    
    // Check NeoPixel event timeout
    if (g_state.np_prio == PRIORITY_EVENT && np_event_deadline_ms > 0) {
        if (now_ms >= np_event_deadline_ms) {
            printf("[CONTROLLER] NeoPixel event timeout, returning to BASE\n");
            g_state.np_prio = PRIORITY_BASE;
            np_event_deadline_ms = 0;
            need_reapply = true;
        }
    }
    
    // Check button event timeout
    if (g_state.btn_prio == PRIORITY_EVENT && btn_event_deadline_ms > 0) {
        if (now_ms >= btn_event_deadline_ms) {
            printf("[CONTROLLER] Button event timeout, returning to BASE\n");
            g_state.btn_prio = PRIORITY_BASE;
            btn_event_deadline_ms = 0;
            need_reapply = true;
        }
    }
    
    // Apply base profile once if any timeout occurred
    if (need_reapply) {
        controller_apply_base_profile();
    }
}

// Manual override functions
void controller_neopixel_override(LedEffect effect) {
    g_state.np_prio = PRIORITY_CRITICAL;
    neopixel_start_effect(effect);
    printf("[CONTROLLER] NeoPixel override: effect %d\n", effect);
}

void controller_neopixel_clear_override(void) {
    if (g_state.np_prio == PRIORITY_CRITICAL) {
        g_state.np_prio = PRIORITY_BASE;
        controller_apply_base_profile();
        printf("[CONTROLLER] NeoPixel override cleared\n");
    }
}

void controller_button_override(ButtonLEDEffect effect) {
    g_state.btn_prio = PRIORITY_CRITICAL;
    buttons_start_effect(effect);
    printf("[CONTROLLER] Button override: effect %d\n", effect);
}

void controller_button_clear_override(void) {
    if (g_state.btn_prio == PRIORITY_CRITICAL) {
        g_state.btn_prio = PRIORITY_BASE;
        controller_apply_base_profile();
        printf("[CONTROLLER] Button override cleared\n");
    }
}
