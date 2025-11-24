#include "game.h"
#include "constants.h"
#include "physics.h"

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
}

void Game_StartGame(GameStruct *game, Bumper *bumpers) {
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
    
    inputSetScore(game->input, 0);
    inputSetGameState(game->input, STATE_GAME);
    inputSetNumBalls(game->input, game->numLives);
    
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

void Game_Update(GameStruct *game,
                 Bumper *bumpers,
                 InputManager *input,
                 ScoreHelper *scores,
                 SoundManager *sound,
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
                Game_StartGame(game, bumpers);
                break;
            case TRANSITION_TO_MENU:
                game->gameState = 0;
                game->currentScene = SCENE_MENU;
                break;
            case TRANSITION_GAME_OVER:
                game->gameState = 2;
                game->currentScene = SCENE_GAME_OVER;
                game->nameSelectIndex = 0;
                game->nameSelectDone = 0;
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
    if (game->powerupScoreDisplay >= powerupTargetScore) {
        game->powerupScore = 0;
        game->waterHeightTarget = 0.5f;
        game->waterHeightTimer = 400.0f;
        game->waterPowerupState = 1;
        playWater(sound);
        
        game->gameScore += 1000;
        if (game->waterPowerupState == 0) {
            game->powerupScore += 1000;
        }
    }
}
