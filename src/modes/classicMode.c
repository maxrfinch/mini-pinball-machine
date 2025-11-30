#include "classicMode.h"
#include "../gameStruct.h"
#include "../inputManager.h"

// ---------------------------------------------------------------------------
// Classic Mode - Traditional pinball experience
// ---------------------------------------------------------------------------

// Initialize classic mode game state
static void classic_init(GameStruct *game, Bumper *bumpers) {
    game->gameState = 1;
    game->currentScene = SCENE_GAME;
    game->numLives = 3;
    game->gameScore = 0;
    game->oldGameScore = 0;
    game->powerupScore = 0;
    game->powerupScoreDisplay = 0;
    game->bumperPowerupState = 0;
    game->ballPowerupState = 0;
    game->waterHeight = 0.0f;
    game->waterPowerupState = 0;
    game->redPowerupOverlay = 0;
    game->bluePowerupOverlay = 0;
    game->slowMotion = 0;
    game->slowMotionCounter = 0;
    game->leftFlipperState = 0;
    game->rightFlipperState = 0;
    game->ballReadyEventSent = 0;
    
    // Initialize slow-mo powerup cooldown state (available at game start)
    game->slowMoPowerupAvailable = 1;
    game->slowMoCooldownTimer = 0.0f;
    game->slowMoCooldownBaselineLives = game->numLives;
    game->slowMoExplosionEffect = 0.0f;
    
    // Initialize launch charge state
    game->launchCharging = 0;
    game->launchChargeAmount = 0.0f;
    game->launchChargeTime = 0.0f;
    
    // Enable/disable bumpers for game start
    bumpers[4].enabled = 1;
    bumpers[5].enabled = 1;
    bumpers[6].enabled = 1;
    bumpers[7].enabled = 1;
    bumpers[8].enabled = 1;
    bumpers[9].enabled = 1;
    bumpers[10].enabled = 0;
    bumpers[11].enabled = 0;
    bumpers[12].enabled = 0;
    bumpers[13].enabled = 0;
}

// Clean up classic mode specific data
static void classic_shutdown(GameStruct *game) {
    game->modeData = NULL;
}

// ---------------------------------------------------------------------------
// Classic Mode Configuration
// ---------------------------------------------------------------------------

const GameModeConfig MODE_CONFIG_CLASSIC = {
    // Identity
    .name = "CLASSIC",
    .description = "Traditional pinball experience",
    
    // Lifecycle functions
    .init = classic_init,
    .shutdown = classic_shutdown,
    
    // Gameplay hooks (NULL = use defaults)
    .update = NULL,
    .renderOverlay = NULL,
    .onBumperHit = NULL,
    
    // Menu theming (current colors from Render_Gameplay background)
    .menuTheme = {
        .backgroundColor = {40, 1, 42, 255},
        .textColor = {255, 255, 255, 255},   // WHITE
        .accentColor = {255, 183, 0, 255}
    },
    
    // Score tracking
    .scoreTableName = "scores_classic",
    
    // Default settings
    .defaultLives = 3,
    .scoreMultiplier = 1.0f
};
