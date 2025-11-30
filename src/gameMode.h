#ifndef HEADER_GAME_MODE
#define HEADER_GAME_MODE

#include "gameStruct.h"

// ---------------------------------------------------------------------------
// Function pointer typedefs for mode lifecycle hooks
// ---------------------------------------------------------------------------

// Initialize game state and bumper configuration for this mode
typedef void (*ModeInitFunc)(GameStruct *game, Bumper *bumpers);

// Clean up mode-specific data
typedef void (*ModeShutdownFunc)(GameStruct *game);

// Optional per-frame mode logic
typedef void (*ModeUpdateFunc)(GameStruct *game, float dt);

// Optional additional rendering (called at end of Render_Gameplay)
typedef void (*ModeRenderOverlayFunc)(const GameStruct *game, const void *resources, float shaderSeconds);

// Optional custom scoring/effects on bumper hit
typedef void (*ModeOnBumperHitFunc)(GameStruct *game, Bumper *bumper, int bumperIndex);

// ---------------------------------------------------------------------------
// Menu theme structure for mode-specific menu styling
// ---------------------------------------------------------------------------

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
    unsigned char a;
} ModeColor;

typedef struct {
    ModeColor backgroundColor;
    ModeColor textColor;
    ModeColor accentColor;
} MenuTheme;

// ---------------------------------------------------------------------------
// Game mode configuration structure
// ---------------------------------------------------------------------------

typedef struct {
    // Identity
    const char *name;
    const char *description;
    
    // Lifecycle function pointers
    ModeInitFunc init;
    ModeShutdownFunc shutdown;
    
    // Gameplay hooks (optional - NULL means use defaults)
    ModeUpdateFunc update;
    ModeRenderOverlayFunc renderOverlay;
    ModeOnBumperHitFunc onBumperHit;
    
    // Menu theming data
    MenuTheme menuTheme;
    
    // Score tracking
    const char *scoreTableName;
    
    // Default settings
    int defaultLives;
    float scoreMultiplier;
} GameModeConfig;

// ---------------------------------------------------------------------------
// Helper functions
// ---------------------------------------------------------------------------

// Get the configuration for a specific game mode
const GameModeConfig *GetModeConfig(GameMode mode);

// Get the total number of registered game modes
int GetNumModes(void);

#endif // HEADER_GAME_MODE
