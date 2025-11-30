#include "game.h"
#include "constants.h"
#include "physics.h"
#include "gameMode.h"
#include "resources.h"

void Game_Init(GameStruct *game, Bumper *bumpers) {
    game->currentScene = SCENE_RAYLIB_TITLE;
    game->nextScene = SCENE_RAYLIB_TITLE;
    game->currentMode = MODE_CLASSIC;
    game->pendingMode = MODE_CLASSIC;
    game->gameState = 5;  // Legacy: raylib title
    
    game->transitionState = 0;
    game->transitionAlpha = 0;
    game->transitionTarget = TRANSITION_TO_MENU;
    game->ballReadyEventSent = 0;
    
    // Initialize mode-specific data pointer
    game->modeData = NULL;
    
    // Initialize water/powerup state to prevent menu activation
    game->powerupScore = 0;
    game->powerupScoreDisplay = 0;
    game->waterHeight = 0.0f;
    game->waterHeightTarget = 0.0f;
    game->waterPowerupState = 0;
    game->waterHeightTimer = 0.0f;
    game->bumperPowerupState = 0;
    game->ballPowerupState = 0;
    game->slowMotion = 0;
    game->slowMotionCounter = 0;
}

void Game_StartGame(GameStruct *game, Bumper *bumpers, Resources *res) {
    const GameModeConfig *config = GetModeConfig(game->pendingMode);
    game->currentMode = game->pendingMode;
    
    // Load mode-specific textures
    Resources_LoadModeTextures(res, config);
    
    if (config->init) {
        config->init(game, bumpers);
    }
    
    inputSetScore(game->input, 0);
    inputSetGameState(game->input, STATE_GAME);
    inputSetNumBalls(game->input, game->numLives);
}

void Game_SpawnBall(GameStruct *game, float x, float y, float vx, float vy, int type) {
    // Wrapper around physics_add_ball for cleaner API
    physics_add_ball(game, x, y, vx, vy, type);
}

void Game_SpawnMultiBalls(GameStruct *game, float x, float y, int count, float vx, float vy, int type) {
    // Spawn multiple balls, each offset vertically by ballSize
    for (int i = 0; i < count; i++) {
        physics_add_ball(game, x, y - (i * ballSize), vx, vy, type);
    }
}

void Game_Update(GameStruct *game,
                 Bumper *bumpers,
                 InputManager *input,
                 ScoreHelper *scores,
                 SoundManager *sound,
                 Resources *res,
                 float dt) {
    
    // Handle transition state machine
    if (game->transitionState == 1) {
        // TRANSITION OUT
        game->transitionAlpha += 15;
        if (game->transitionAlpha >= 255) {
            game->transitionState = 2;
            game->transitionAlpha = 255;
            game->transitionDelay = 0;
        }
    } else if (game->transitionState == 2) {
        // HANDLE LOAD
        switch (game->transitionTarget) {
            case TRANSITION_TO_GAME:
                Game_StartGame(game, bumpers, res);
                break;
            case TRANSITION_TO_MENU:
                // Ensure camera is stopped when leaving game over scenes
                if (game->camera.preview_active) {
                    Camera_StopPreview(&game->camera);
                }
                game->gameState = 0;
                game->currentScene = SCENE_MENU;
                break;
            case TRANSITION_GAME_OVER:
                game->gameState = 2;
                game->currentScene = SCENE_GAME_OVER;
                game->nameSelectIndex = 0;
                game->nameSelectDone = 0;
                break;
            case TRANSITION_GAME_OVER_TOP3:
                game->gameState = 3;  // New game state for top 3 game over
                game->currentScene = SCENE_GAME_OVER_TOP3;
                game->nameSelectIndex = 0;
                game->nameSelectDone = 0;
                
                // Start camera preview for Top-3 Game Over (if not already pre-warmed during gameplay)
                if (game->camera.initialized && !game->camera.preview_active) {
                    Camera_StartPreview(&game->camera);
                }
                
                // Send NeoPixel camera preview command
                if (game->input) {
                    inputSendCameraPreview(game->input);
                }
                break;
        }
        game->transitionDelay++;
        if (game->transitionDelay > 10) {
            game->transitionState = 3;
        }
    } else if (game->transitionState == 3) {
        // TRANSITION IN
        game->transitionAlpha -= 15;
        if (game->transitionAlpha <= 0) {
            game->transitionState = 0;
            game->transitionAlpha = 0;
        }
    } else {
        game->transitionAlpha = 0;
    }
    
    // Mode update hook - call mode-specific update logic if defined
    const GameModeConfig *config = GetModeConfig(game->currentMode);
    if (config->update) {
        config->update(game, dt);
    }
    
    // Handle raylib title screen transition
    if (game->gameState == 5) {
        if (game->transitionState == 0) {
            game->transitionState = 1;
            game->transitionTarget = TRANSITION_TO_MENU;
        }
    }
    
    // Update bumper bounce effects
    for (int i = 0; i < numBumpers; i++) {
        bumpers[i].bounceEffect *= 0.94;
        if (bumpers[i].enabled) {
            bumpers[i].enabledSize += 0.1f;
            if (bumpers[i].enabledSize > 1.0f) {
                bumpers[i].enabledSize = 1.0f;
            }
        } else {
            bumpers[i].enabledSize -= 0.1f;
            if (bumpers[i].enabledSize < 0.0f) {
                bumpers[i].enabledSize = 0.0f;
            }
        }
    }
    
    // Update powerup score display
    if (game->powerupScoreDisplay < game->powerupScore) {
        game->powerupScoreDisplay += 10;
        if (game->powerupScoreDisplay > game->powerupScore) {
            game->powerupScoreDisplay = game->powerupScore;
        }
    } else if (game->powerupScoreDisplay > game->powerupScore) {
        game->powerupScoreDisplay -= 20;
        if (game->powerupScoreDisplay < game->powerupScore) {
            game->powerupScoreDisplay = game->powerupScore;
        }
    }
    if (game->powerupScoreDisplay < 0) {
        game->powerupScoreDisplay = 0;
    }
    
    // If the powerup is full, dispense water powerup
    // Rule 2: If Slow-Mo is active, Water powerup will not activate
    // Only activate water powerup during gameplay (gameState == 1)
    if (game->gameState == 1 && game->powerupScoreDisplay >= powerupTargetScore && game->slowMotion == 0) {
        game->powerupScore = 0;
        game->waterHeightTarget = 0.5f;
        game->waterHeightTimer = 400.0f;
        game->waterPowerupState = 1;
        playWater(sound);
        
        // Start water neo and matrix effect
        inputSendWaterEffectStart(input);
        
        game->gameScore += 1000;
        if (game->waterPowerupState == 0) {
            game->powerupScore += 1000;
        }
    }
}
