/**
 * debug_mode.h
 * 
 * Debug mode implementation
 */

#ifndef DEBUG_MODE_H
#define DEBUG_MODE_H

#include <stdbool.h>

// Initialize debug mode subsystem
void debug_mode_init(void);

// Check and enter debug mode if needed
void debug_mode_check(void);

// Exit debug mode
void debug_mode_exit(void);

// Update debug animations
void debug_mode_update(void);

// Check if in debug mode
bool debug_mode_is_active(void);

#endif // DEBUG_MODE_H
