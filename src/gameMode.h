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
// Mode-specific texture paths
// ---------------------------------------------------------------------------

typedef struct {
    // Gameplay textures
    const char *backgroundTexture;
    const char *ballTexture;
    const char *beachBallTexture;
    const char *trailTexture;
    const char *flipperLeftTexture;
    const char *flipperRightTexture;
    const char *bumperTexture;
    const char *bumperLightTexture;
    const char *iceBumperTexture;
    const char *bumper3Texture;
    const char *shockwaveTexture;
    const char *lowerBumperShockTexture;
    
    // Menu textures
    const char *menuBackgroundTexture;
    const char *titleOverlayTexture;
    const char *menuOverlaySystemTexture;
    const char *menuOverlayControlsTexture;
    const char *menuOverlayHighscoresTexture;
    const char *menuOverlayTopScoresTexture;
    
    // Game over textures
    const char *gameOverOverlay1Texture;
    const char *gameOverOverlay2Texture;
    const char *gameOverOverlay1Top3Texture;
    const char *gameOverOverlay2Top3Texture;
    
    // Powerup/effect textures
    const char *waterTexture;
    const char *waterOverlayTexture;
    const char *iceOverlayTexture;
    const char *redPowerupOverlayTexture;
    const char *particleTexture;
    
    // UI textures
    const char *arrowRightTexture;
    const char *menuControlsTexture;
    const char *transitionTexture;
    const char *debugTexture;
} ModeTextures;

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
    
    // Mode-specific texture paths
    ModeTextures textures;
    
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
