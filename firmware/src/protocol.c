/**
 * protocol.c
 * 
 * Command protocol parser (Pi-centric architecture)
 * Pi sends direct effect/display commands, controller executes them
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "protocol.h"
#include "neopixel.h"
#include "buttons.h"
#include "display.h"
#include "debug_mode.h"
#include "hardware_config.h"
#include "controller_state.h"

#define CMD_BUFFER_SIZE 128

static char cmd_buffer[CMD_BUFFER_SIZE];
static uint8_t cmd_buffer_pos = 0;
static absolute_time_t last_command_time = 0;

void protocol_init(void) {
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
    cmd_buffer_pos = 0;
    last_command_time = get_absolute_time();
}

void protocol_update_activity(void) {
    last_command_time = get_absolute_time();
}

bool protocol_is_debug_timeout(void) {
    absolute_time_t now = get_absolute_time();
    int64_t diff_ms = absolute_time_diff_us(last_command_time, now) / 1000;
    return diff_ms > DEBUG_TIMEOUT_MS;
}

// Helper to parse button target (LEFT, CENTER, RIGHT, ALL)
static bool parse_button_target(const char* str, Button* button, bool* all) {
    if (strcmp(str, "ALL") == 0) {
        *all = true;
        return true;
    } else if (strcmp(str, "LEFT") == 0) {
        *button = BUTTON_LEFT;
        *all = false;
        return true;
    } else if (strcmp(str, "CENTER") == 0) {
        *button = BUTTON_CENTER;
        *all = false;
        return true;
    } else if (strcmp(str, "RIGHT") == 0) {
        *button = BUTTON_RIGHT;
        *all = false;
        return true;
    }
    return false;
}

static void parse_command(const char* cmd) {
    // Update activity timestamp
    protocol_update_activity();
    
    // Exit debug mode if active (unless entering debug mode)
    if (strcmp(cmd, "CMD DEBUG") != 0) {
        debug_mode_exit();
    }
    
    // ===== DISPLAY COMMANDS =====
    if (strncmp(cmd, "CMD DISPLAY SCORE ", 18) == 0) {
        // CMD DISPLAY SCORE <number>
        uint32_t score = atoi(cmd + 18);
        display_set_score(score);
        
    } else if (strncmp(cmd, "CMD DISPLAY BALLS ", 18) == 0) {
        // CMD DISPLAY BALLS <number>
        uint8_t balls = atoi(cmd + 18);
        display_set_balls(balls);
        
    } else if (strncmp(cmd, "CMD DISPLAY TEXT ", 17) == 0) {
        // CMD DISPLAY TEXT <string>
        const char* text = cmd + 17;
        display_clear();
        display_set_text(text, 0, 0);
        
    } else if (strcmp(cmd, "CMD DISPLAY CLEAR") == 0) {
        // CMD DISPLAY CLEAR
        display_clear();
        
    } else if (strncmp(cmd, "CMD DISPLAY ", 12) == 0) {
        // CMD DISPLAY <ANIMATION_NAME>
        const char* anim_name = cmd + 12;
        
        if (strcmp(anim_name, "BALL_SAVED") == 0) {
            display_start_animation(DISPLAY_ANIM_BALL_SAVED);
        } else if (strcmp(anim_name, "MULTIBALL") == 0) {
            display_start_animation(DISPLAY_ANIM_MULTIBALL);
        } else if (strcmp(anim_name, "MAIN_MENU") == 0) {
            display_start_animation(DISPLAY_ANIM_MAIN_MENU);
        }
        
    // ===== NEOPIXEL EFFECT COMMANDS =====
    } else if (strncmp(cmd, "CMD NEO EFFECT ", 15) == 0) {
        const char* effect = cmd + 15;
        
        if (strcmp(effect, "RAINBOW_BREATHE") == 0) {
            controller_neopixel_set_effect(EFFECT_RAINBOW_BREATHE, PRIORITY_BASE);
        } else if (strcmp(effect, "RAINBOW_WAVE") == 0) {
            controller_neopixel_set_effect(EFFECT_RAINBOW_WAVE, PRIORITY_BASE);
        } else if (strcmp(effect, "CAMERA_FLASH") == 0) {
            controller_neopixel_set_effect(EFFECT_CAMERA_FLASH, PRIORITY_BASE);
        } else if (strcmp(effect, "RED_STROBE_5X") == 0) {
            controller_neopixel_set_effect(EFFECT_RED_STROBE_5X, PRIORITY_BASE);
        } else if (strcmp(effect, "WATER") == 0) {
            controller_neopixel_set_effect(EFFECT_WATER, PRIORITY_BASE);
        } else if (strcmp(effect, "ATTRACT") == 0) {
            controller_neopixel_set_effect(EFFECT_ATTRACT, PRIORITY_BASE);
        } else if (strcmp(effect, "PINK_PULSE") == 0) {
            controller_neopixel_set_effect(EFFECT_PINK_PULSE, PRIORITY_BASE);
        } else if (strcmp(effect, "BALL_LAUNCH") == 0) {
            controller_neopixel_set_effect(EFFECT_BALL_LAUNCH, PRIORITY_BASE);
        } else if (strcmp(effect, "NONE") == 0) {
            controller_neopixel_set_effect(EFFECT_NONE, PRIORITY_BASE);
        }
        
    } else if (strcmp(cmd, "CMD NEO EFFECT CLEAR") == 0) {
        // CMD NEO EFFECT CLEAR
        controller_neopixel_set_effect(EFFECT_NONE, PRIORITY_BASE);
        
    } else if (strncmp(cmd, "CMD NEO BRIGHTNESS ", 19) == 0) {
        // CMD NEO BRIGHTNESS <0-255>
        uint8_t brightness = atoi(cmd + 19);
        neopixel_set_brightness(brightness);
        
    // ===== BUTTON EFFECT COMMANDS =====
    } else if (strncmp(cmd, "CMD BUTTON EFFECT ", 18) == 0) {
        // CMD BUTTON EFFECT <LEFT|CENTER|RIGHT|ALL> <EFFECT_NAME>
        const char* args = cmd + 18;
        char target[16] = {0};
        const char* effect = NULL;
        
        // Parse target button
        int i = 0;
        while (args[i] && args[i] != ' ' && i < 15) {
            target[i] = args[i];
            i++;
        }
        target[i] = '\0';
        
        // Skip space to find effect name
        if (args[i] == ' ') {
            effect = args + i + 1;
        }
        
        if (effect && *effect) {
            Button button;
            bool all = false;
            
            if (parse_button_target(target, &button, &all)) {
                ButtonLEDEffect btn_effect = BTN_EFFECT_OFF;
                bool valid_effect = true;
                
                if (strcmp(effect, "OFF") == 0) {
                    btn_effect = BTN_EFFECT_OFF;
                } else if (strcmp(effect, "READY_STEADY_GLOW") == 0) {
                    btn_effect = BTN_EFFECT_READY_STEADY_GLOW;
                } else if (strcmp(effect, "FLIPPER_FEEDBACK") == 0) {
                    btn_effect = BTN_EFFECT_FLIPPER_FEEDBACK;
                } else if (strcmp(effect, "CENTER_HIT_PULSE") == 0) {
                    btn_effect = BTN_EFFECT_CENTER_HIT_PULSE;
                } else if (strcmp(effect, "SKILL_SHOT_BUILDUP") == 0) {
                    btn_effect = BTN_EFFECT_SKILL_SHOT_BUILDUP;
                } else if (strcmp(effect, "BALL_SAVED") == 0) {
                    btn_effect = BTN_EFFECT_BALL_SAVED;
                } else if (strcmp(effect, "POWERUP_ALERT") == 0) {
                    btn_effect = BTN_EFFECT_POWERUP_ALERT;
                } else if (strcmp(effect, "EXTRA_BALL_AWARD") == 0) {
                    btn_effect = BTN_EFFECT_EXTRA_BALL_AWARD;
                } else if (strcmp(effect, "GAME_OVER_FADE") == 0) {
                    btn_effect = BTN_EFFECT_GAME_OVER_FADE;
                } else if (strcmp(effect, "MENU_NAVIGATION") == 0) {
                    btn_effect = BTN_EFFECT_MENU_NAVIGATION;
                } else {
                    valid_effect = false;
                }
                
                if (valid_effect) {
                    if (all) {
                        controller_button_set_effect_all(btn_effect, PRIORITY_BASE);
                    } else {
                        controller_button_set_effect_single(button, btn_effect, PRIORITY_BASE);
                    }
                }
            }
        }
        
    } else if (strcmp(cmd, "CMD BUTTON EFFECT CLEAR") == 0) {
        // CMD BUTTON EFFECT CLEAR
        controller_button_set_effect_all(BTN_EFFECT_OFF, PRIORITY_BASE);
        
    // ===== TEMPORARY EVENT EFFECTS (convenience commands) =====
    } else if (strncmp(cmd, "CMD EVENT ", 10) == 0) {
        const char* event = cmd + 10;
        
        if (strcmp(event, "BALL_SAVED") == 0) {
            controller_neopixel_play_one_shot(EFFECT_RED_STROBE_5X, PRIORITY_EVENT, 1500);
            controller_button_play_one_shot(BTN_EFFECT_BALL_SAVED, PRIORITY_EVENT, 1500);
        } else if (strcmp(event, "EXTRA_BALL") == 0) {
            controller_neopixel_play_one_shot(EFFECT_PINK_PULSE, PRIORITY_EVENT, 2000);
            controller_button_play_one_shot(BTN_EFFECT_EXTRA_BALL_AWARD, PRIORITY_EVENT, 2000);
        } else if (strcmp(event, "JACKPOT") == 0) {
            controller_neopixel_play_one_shot(EFFECT_RAINBOW_WAVE, PRIORITY_EVENT, 2500);
            controller_button_play_one_shot(BTN_EFFECT_POWERUP_ALERT, PRIORITY_EVENT, 2500);
        } else if (strcmp(event, "MULTIBALL_START") == 0) {
            controller_neopixel_play_one_shot(EFFECT_PINK_PULSE, PRIORITY_EVENT, 2000);
            controller_button_play_one_shot(BTN_EFFECT_POWERUP_ALERT, PRIORITY_EVENT, 2000);
        }
        
    // ===== SYSTEM COMMANDS =====
    } else if (strcmp(cmd, "CMD PING") == 0) {
        // CMD PING
        protocol_send_pong();
        
    } else if (strcmp(cmd, "CMD DEBUG") == 0) {
        // CMD DEBUG
        debug_mode_enter();
    }
}

void protocol_process(void) {
    // Read available characters from USB CDC
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\n' || c == '\r') {
            // End of command
            if (cmd_buffer_pos > 0) {
                cmd_buffer[cmd_buffer_pos] = '\0';
                parse_command(cmd_buffer);
                cmd_buffer_pos = 0;
            }
        } else if (cmd_buffer_pos < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_buffer_pos++] = (char)c;
        }
    }
}

void protocol_send_button_event(Button button, ButtonState state) {
    const char* button_name;
    const char* state_name;
    
    switch (button) {
        case BUTTON_LEFT: button_name = "LEFT"; break;
        case BUTTON_CENTER: button_name = "CENTER"; break;
        case BUTTON_RIGHT: button_name = "RIGHT"; break;
        default: return;
    }
    
    switch (state) {
        case BUTTON_STATE_UP: state_name = "UP"; break;
        case BUTTON_STATE_DOWN: state_name = "DOWN"; break;
        case BUTTON_STATE_HELD: state_name = "HELD"; break;
        default: return;
    }
    
    printf("EVT BUTTON %s %s\n", button_name, state_name);
}

void protocol_send_pong(void) {
    printf("EVT PONG\n");
}

void protocol_send_ready(void) {
    printf("EVT READY\n");
}

void protocol_send_debug_active(void) {
    printf("EVT DEBUG ACTIVE\n");
}
