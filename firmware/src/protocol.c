/**
 * protocol.c
 * 
 * Command protocol parser and event system
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

static void parse_command(const char* cmd) {
    // Update activity timestamp
    protocol_update_activity();
    
    // Exit debug mode if active (unless entering debug mode)
    if (strcmp(cmd, "CMD DEBUG") != 0) {
        debug_mode_exit();
    }
    
    // Parse command
    // ===== STATE COMMANDS (long-lived configuration) =====
    if (strncmp(cmd, "CMD MODE ", 9) == 0) {
        // CMD MODE <ATTRACT|MENU|GAME|BALL_LOST|HIGH_SCORE|DEBUG>
        const char* mode = cmd + 9;
        
        if (strcmp(mode, "ATTRACT") == 0) {
            controller_set_mode(MODE_ATTRACT);
        } else if (strcmp(mode, "MENU") == 0) {
            controller_set_mode(MODE_MENU);
        } else if (strcmp(mode, "GAME") == 0) {
            controller_set_mode(MODE_GAME);
        } else if (strcmp(mode, "BALL_LOST") == 0) {
            controller_set_mode(MODE_BALL_LOST);
        } else if (strcmp(mode, "HIGH_SCORE") == 0) {
            controller_set_mode(MODE_HIGH_SCORE);
        } else if (strcmp(mode, "DEBUG") == 0) {
            controller_set_mode(MODE_DEBUG);
        }
        
    } else if (strncmp(cmd, "CMD STATE ", 10) == 0) {
        // CMD STATE <flag> <value>
        const char* rest = cmd + 10;
        
        if (strncmp(rest, "BALL_READY ", 11) == 0) {
            uint8_t value = atoi(rest + 11);
            controller_set_ball_ready(value != 0);
        } else if (strncmp(rest, "SKILL_SHOT ", 11) == 0) {
            uint8_t value = atoi(rest + 11);
            controller_set_skill_shot(value != 0);
        } else if (strncmp(rest, "MULTIBALL ", 10) == 0) {
            uint8_t value = atoi(rest + 10);
            controller_set_multiball(value != 0);
        }
        
    } else if (strncmp(cmd, "CMD MENU_MODE ", 14) == 0) {
        // CMD MENU_MODE <SMART|DUMB>
        const char* mode = cmd + 14;
        
        if (strcmp(mode, "SMART") == 0) {
            controller_set_menu_mode(true);
        } else if (strcmp(mode, "DUMB") == 0) {
            controller_set_menu_mode(false);
        }
        
    } else if (strncmp(cmd, "CMD MENU_SIZE ", 14) == 0) {
        // CMD MENU_SIZE <n>
        uint8_t size = atoi(cmd + 14);
        controller_set_menu_size(size);
        
    } else if (strncmp(cmd, "CMD MENU_INDEX ", 15) == 0) {
        // CMD MENU_INDEX <i>
        uint8_t index = atoi(cmd + 15);
        controller_set_menu_index(index);
        
    } else if (strncmp(cmd, "CMD SCORE ", 10) == 0) {
        // CMD SCORE <number>
        uint32_t score = atoi(cmd + 10);
        display_set_score(score);
        
    } else if (strncmp(cmd, "CMD BALLS ", 10) == 0) {
        // CMD BALLS <number>
        uint8_t balls = atoi(cmd + 10);
        display_set_balls(balls);
        
    } else if (strncmp(cmd, "CMD EFFECT ", 11) == 0) {
        // CMD EFFECT <pattern_name>
        const char* effect = cmd + 11;
        
        if (strcmp(effect, "RAINBOW_BREATHE") == 0) {
            neopixel_start_effect(EFFECT_RAINBOW_BREATHE);
        } else if (strcmp(effect, "RAINBOW_WAVE") == 0) {
            neopixel_start_effect(EFFECT_RAINBOW_WAVE);
        } else if (strcmp(effect, "CAMERA_FLASH") == 0) {
            neopixel_start_effect(EFFECT_CAMERA_FLASH);
        } else if (strcmp(effect, "RED_STROBE_5X") == 0) {
            neopixel_start_effect(EFFECT_RED_STROBE_5X);
        } else if (strcmp(effect, "WATER") == 0) {
            neopixel_start_effect(EFFECT_WATER);
        } else if (strcmp(effect, "ATTRACT") == 0) {
            neopixel_start_effect(EFFECT_ATTRACT);
        } else if (strcmp(effect, "PINK_PULSE") == 0) {
            neopixel_start_effect(EFFECT_PINK_PULSE);
        } else if (strcmp(effect, "BALL_LAUNCH") == 0) {
            neopixel_start_effect(EFFECT_BALL_LAUNCH);
        }
        
    } else if (strncmp(cmd, "CMD BUTTON_EFFECT ", 18) == 0) {
        // CMD BUTTON_EFFECT <effect_name>
        const char* effect = cmd + 18;
        
        if (strcmp(effect, "OFF") == 0) {
            buttons_start_effect(BTN_EFFECT_OFF);
        } else if (strcmp(effect, "READY_STEADY_GLOW") == 0) {
            buttons_start_effect(BTN_EFFECT_READY_STEADY_GLOW);
        } else if (strcmp(effect, "FLIPPER_FEEDBACK") == 0) {
            buttons_start_effect(BTN_EFFECT_FLIPPER_FEEDBACK);
        } else if (strcmp(effect, "CENTER_HIT_PULSE") == 0) {
            buttons_start_effect(BTN_EFFECT_CENTER_HIT_PULSE);
        } else if (strcmp(effect, "SKILL_SHOT_BUILDUP") == 0) {
            buttons_start_effect(BTN_EFFECT_SKILL_SHOT_BUILDUP);
        } else if (strcmp(effect, "BALL_SAVED") == 0) {
            buttons_start_effect(BTN_EFFECT_BALL_SAVED);
        } else if (strcmp(effect, "POWERUP_ALERT") == 0) {
            buttons_start_effect(BTN_EFFECT_POWERUP_ALERT);
        } else if (strcmp(effect, "EXTRA_BALL_AWARD") == 0) {
            buttons_start_effect(BTN_EFFECT_EXTRA_BALL_AWARD);
        } else if (strcmp(effect, "GAME_OVER_FADE") == 0) {
            buttons_start_effect(BTN_EFFECT_GAME_OVER_FADE);
        } else if (strcmp(effect, "MENU_NAVIGATION") == 0) {
            buttons_start_effect(BTN_EFFECT_MENU_NAVIGATION);
        }
        
    } else if (strncmp(cmd, "CMD BRIGHTNESS ", 15) == 0) {
        // CMD BRIGHTNESS <0-255>
        uint8_t brightness = atoi(cmd + 15);
        neopixel_set_brightness(brightness);
        
    // ===== EVENT COMMANDS (short-lived game events) =====
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
            controller_set_multiball(true);
            controller_neopixel_play_one_shot(EFFECT_PINK_PULSE, PRIORITY_EVENT, 2000);
            controller_button_play_one_shot(BTN_EFFECT_POWERUP_ALERT, PRIORITY_EVENT, 2000);
        } else if (strcmp(event, "MULTIBALL_END") == 0) {
            controller_set_multiball(false);
        }
        
    // ===== OVERRIDE COMMANDS (manual control) =====
    } else if (strncmp(cmd, "CMD EFFECT_OVERRIDE ", 20) == 0) {
        // CMD EFFECT_OVERRIDE <NEO_EFFECT_NAME>
        const char* effect = cmd + 20;
        
        if (strcmp(effect, "RAINBOW_BREATHE") == 0) {
            controller_neopixel_override(EFFECT_RAINBOW_BREATHE);
        } else if (strcmp(effect, "RAINBOW_WAVE") == 0) {
            controller_neopixel_override(EFFECT_RAINBOW_WAVE);
        } else if (strcmp(effect, "CAMERA_FLASH") == 0) {
            controller_neopixel_override(EFFECT_CAMERA_FLASH);
        } else if (strcmp(effect, "RED_STROBE_5X") == 0) {
            controller_neopixel_override(EFFECT_RED_STROBE_5X);
        } else if (strcmp(effect, "WATER") == 0) {
            controller_neopixel_override(EFFECT_WATER);
        } else if (strcmp(effect, "ATTRACT") == 0) {
            controller_neopixel_override(EFFECT_ATTRACT);
        } else if (strcmp(effect, "PINK_PULSE") == 0) {
            controller_neopixel_override(EFFECT_PINK_PULSE);
        } else if (strcmp(effect, "BALL_LAUNCH") == 0) {
            controller_neopixel_override(EFFECT_BALL_LAUNCH);
        } else if (strcmp(effect, "NONE") == 0) {
            controller_neopixel_override(EFFECT_NONE);
        }
        
    } else if (strncmp(cmd, "CMD BUTTON_EFFECT_OVERRIDE ", 27) == 0) {
        // CMD BUTTON_EFFECT_OVERRIDE <BTN_EFFECT_NAME>
        const char* effect = cmd + 27;
        
        if (strcmp(effect, "OFF") == 0) {
            controller_button_override(BTN_EFFECT_OFF);
        } else if (strcmp(effect, "READY_STEADY_GLOW") == 0) {
            controller_button_override(BTN_EFFECT_READY_STEADY_GLOW);
        } else if (strcmp(effect, "FLIPPER_FEEDBACK") == 0) {
            controller_button_override(BTN_EFFECT_FLIPPER_FEEDBACK);
        } else if (strcmp(effect, "CENTER_HIT_PULSE") == 0) {
            controller_button_override(BTN_EFFECT_CENTER_HIT_PULSE);
        } else if (strcmp(effect, "SKILL_SHOT_BUILDUP") == 0) {
            controller_button_override(BTN_EFFECT_SKILL_SHOT_BUILDUP);
        } else if (strcmp(effect, "BALL_SAVED") == 0) {
            controller_button_override(BTN_EFFECT_BALL_SAVED);
        } else if (strcmp(effect, "POWERUP_ALERT") == 0) {
            controller_button_override(BTN_EFFECT_POWERUP_ALERT);
        } else if (strcmp(effect, "EXTRA_BALL_AWARD") == 0) {
            controller_button_override(BTN_EFFECT_EXTRA_BALL_AWARD);
        } else if (strcmp(effect, "GAME_OVER_FADE") == 0) {
            controller_button_override(BTN_EFFECT_GAME_OVER_FADE);
        } else if (strcmp(effect, "MENU_NAVIGATION") == 0) {
            controller_button_override(BTN_EFFECT_MENU_NAVIGATION);
        }
        
    } else if (strcmp(cmd, "CMD EFFECT_CLEAR") == 0) {
        // CMD EFFECT_CLEAR
        controller_neopixel_clear_override();
        
    } else if (strcmp(cmd, "CMD BUTTON_EFFECT_CLEAR") == 0) {
        // CMD BUTTON_EFFECT_CLEAR
        controller_button_clear_override();
        
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
