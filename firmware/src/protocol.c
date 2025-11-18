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
    // STATE command
    else if (strcmp(token, "STATE") == 0) {
        token = strtok(NULL, " ");
        if (token != NULL) {
            int state_id = atoi(token);
            if (state_id >= GAME_STATE_MENU && state_id <= GAME_STATE_GAME_OVER) {
                current_state = (GameState)state_id;
            }
        }
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
    else if (strcmp(token, "BTN_LED") == 0) {
        // TODO: Phase 2 - Button LED control
    }
    else if (strcmp(token, "HAPT") == 0) {
        // TODO: Phase 3 - Haptic motor control
    }
    // Unknown commands are safely ignored
}

GameState protocol_get_state(void) {
    return current_state;
}
