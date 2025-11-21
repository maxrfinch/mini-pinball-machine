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
    if (strncmp(cmd, "CMD MODE ", 9) == 0) {
        // CMD MODE <ATTRACT|MENU|GAME|BALL_LOST|HIGH_SCORE>
        const char* mode = cmd + 9;
        // Mode is handled by the game logic, just acknowledge
        
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
