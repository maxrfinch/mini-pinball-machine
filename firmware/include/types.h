/**
 * types.h
 * 
 * Common type definitions and data structures
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stdbool.h>

// LED Board Configuration
typedef enum {
    BOARD_POS_RIGHT_FRONT = 0,
    BOARD_POS_RIGHT_REAR,
    BOARD_POS_CAMERA_BAR,
    BOARD_POS_LEFT_REAR,
    BOARD_POS_LEFT_FRONT,
    BOARD_POS_FRONT_BAR
} BoardPosition;

typedef struct {
    uint8_t id;              // 1-6
    uint8_t start_index;     // LED index in global chain
    uint8_t end_index;       // start + 7
    bool reversed;           // LED direction reversed
    BoardPosition position;  // Physical position
} LedBoard;

// LED Effect Patterns
typedef enum {
    EFFECT_RAINBOW_BREATHE = 0,
    EFFECT_RAINBOW_WAVE,
    EFFECT_CAMERA_FLASH,
    EFFECT_RED_STROBE_5X,
    EFFECT_WATER,
    EFFECT_ATTRACT,
    EFFECT_PINK_PULSE,
    EFFECT_BALL_LAUNCH,
    EFFECT_NONE
} LedEffect;

// Game Modes
typedef enum {
    MODE_ATTRACT = 0,
    MODE_MENU,
    MODE_GAME,
    MODE_BALL_LOST,
    MODE_HIGH_SCORE,
    MODE_DEBUG
} GameMode;

// Button States
typedef enum {
    BUTTON_LEFT = 0,
    BUTTON_CENTER,
    BUTTON_RIGHT
} Button;

typedef enum {
    BUTTON_STATE_UP = 0,
    BUTTON_STATE_DOWN,
    BUTTON_STATE_HELD
} ButtonState;

// RGB Color
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Color;

#endif // TYPES_H
