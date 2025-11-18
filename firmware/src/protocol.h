#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

// Game states (matching new protocol)
typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_GAME = 1,
    GAME_STATE_GAME_OVER = 2
} GameState;

// Initialize protocol handler
void protocol_init(void);

// Process a command line
void protocol_process_command(const char *cmd);

// Get current game state
GameState protocol_get_state(void);

#endif // PROTOCOL_H
