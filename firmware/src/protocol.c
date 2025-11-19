#include "protocol.h"
#include "hw_display.h"
#include "hw_neopixel.h"
#include "hw_button_leds.h"
#include "hw_haptics.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static GameState current_state = GAME_STATE_MENU;

void protocol_init(void) {
    current_state = GAME_STATE_MENU;
}

void protocol_process_command(const char *cmd) {
    if (cmd == NULL || strlen(cmd) == 0) {
        return;
    }
    
    // Parse command
    char cmd_copy[128];
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    
    // Split by space
    char *token = strtok(cmd_copy, " ");
    if (token == NULL) {
        return;
    }
    
    // SCORE command
    if (strcmp(token, "SCORE") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            uint32_t score = (uint32_t)atol(token);
            hw_display_render_score(score);
            hw_display_refresh();
        }
    }
    // BALLS command
    else if (strcmp(token, "BALLS") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            uint8_t balls = (uint8_t)atoi(token);
            hw_display_render_balls(balls);
            hw_display_refresh();
        }
    }
    // STATE command - supports both numeric and text formats
    else if (strcmp(token, "STATE") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            GameState new_state;
            
            // Try text-based state names first
            if (strcmp(token, "MENU") == 0) {
                new_state = GAME_STATE_MENU;
            } else if (strcmp(token, "GAME") == 0) {
                new_state = GAME_STATE_GAME;
            } else if (strcmp(token, "GAME_OVER") == 0) {
                new_state = GAME_STATE_GAME_OVER;
            } else {
                // Fall back to numeric format for backwards compatibility
                int state_id = atoi(token);
                if (state_id >= GAME_STATE_MENU && state_id <= GAME_STATE_GAME_OVER) {
                    new_state = (GameState)state_id;
                } else {
                    return;  // Invalid state
                }
            }
            
            current_state = new_state;
            // Update LED game state to match
            button_leds_set_game_state((LedGameState)new_state);
        }
    }
    // EVENT command
    else if (strcmp(token, "EVENT") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            if (strcmp(token, "GAME_START") == 0) {
                button_leds_on_game_start();
            } else if (strcmp(token, "BALL_READY") == 0) {
                button_leds_on_ball_ready();
            } else if (strcmp(token, "BALL_LAUNCHED") == 0) {
                button_leds_on_ball_launched();
            }
        }
    }
    // BTN_LED command: BTN_LED <idx> <mode> <r> <g> <b> [count]
    // idx: 0=left, 1=center, 2=right
    // mode: 0=off, 1=steady, 2=breathe, 3=blink, 4=strobe
    // r, g, b: color values (0-255)
    // count: optional, for blink/strobe modes (default 1)
    else if (strcmp(token, "BTN_LED") == 0) {
        uint8_t idx = 0, mode = 0, r = 0, g = 0, b = 0, count = 1;
        
        token = strtok(NULL, " ");
        if (token != NULL) idx = (uint8_t)atoi(token);
        
        token = strtok(NULL, " ");
        if (token != NULL) mode = (uint8_t)atoi(token);
        
        token = strtok(NULL, " ");
        if (token != NULL) r = (uint8_t)atoi(token);
        
        token = strtok(NULL, " ");
        if (token != NULL) g = (uint8_t)atoi(token);
        
        token = strtok(NULL, " ");
        if (token != NULL) b = (uint8_t)atoi(token);
        
        token = strtok(NULL, " ");
        if (token != NULL) count = (uint8_t)atoi(token);
        
        hw_button_leds_set(idx, (LEDMode)mode, r, g, b, count);
    }
    // Legacy commands (backwards compatibility)
    else if (strcmp(token, "GAME_START") == 0) {
        button_leds_on_game_start();
    }
    else if (strcmp(token, "BALL_LAUNCH") == 0) {
        button_leds_on_ball_launched();
    }
    // Future commands (Phase 2 & 3) - stubs
    else if (strcmp(token, "NEO_SET") == 0) {
        // TODO: Phase 2 - NeoPixel individual LED set
    }
    else if (strcmp(token, "NEO_FILL") == 0) {
        // TODO: Phase 2 - NeoPixel fill all
    }
    else if (strcmp(token, "NEO_MODE") == 0) {
        // TODO: Phase 2 - NeoPixel animation mode
    }
    else if (strcmp(token, "NEO_POWER_MODE") == 0) {
        // TODO: Phase 2 - Power LED mode
    }
    else if (strcmp(token, "HAPT") == 0) {
        // TODO: Phase 3 - Haptic motor control
    }
    // Unknown commands are safely ignored
}

GameState protocol_get_state(void) {
    return current_state;
}
