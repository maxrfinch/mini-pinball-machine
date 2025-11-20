/**
 * protocol.h
 * 
 * Command protocol parser and event system
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "types.h"

// Initialize protocol handler
void protocol_init(void);

// Process incoming commands
void protocol_process(void);

// Send button event
void protocol_send_button_event(Button button, ButtonState state);

// Send PONG response
void protocol_send_pong(void);

// Send READY event
void protocol_send_ready(void);

// Send DEBUG ACTIVE event
void protocol_send_debug_active(void);

// Update last command time (for debug timeout)
void protocol_update_activity(void);

// Check if debug mode should be active
bool protocol_is_debug_timeout(void);

#endif // PROTOCOL_H
