#include "menu.h"
#include "constants.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "soundManager.h"
#include "camera.h"

void Menu_Init(GameStruct *game, MenuPinball *menuPinballs, int numMenuPinballs) {
    game->menuState = 0;
    game->menuHighScoreTimer = 0;
    
    // Initialize telemetry cache for System Scene
    game->cachedCpuTemp = -1;
    game->cachedBatteryPercent = -1;
    game->lastTempReadTime = 0;
    game->lastBatteryReadTime = 0;
    
    // Initialize menu pinballs
    for (int i = 0; i < numMenuPinballs; i++) {
        menuPinballs[i].px = -100;
        menuPinballs[i].py = (rand() % screenHeight);
        menuPinballs[i].vx = 0;
        menuPinballs[i].vy = 0;
    }
}

void Menu_Update(GameStruct *game,
                 MenuPinball *menuPinballs,
                 int numMenuPinballs,
                 InputManager *input,
                 SoundManager *sound) {
    
    // Update menu pinballs (only first half are actively used)
    for (int i = 0; i < numMenuPinballs / 2; i++) {
        menuPinballs[i].px += menuPinballs[i].vx;
        menuPinballs[i].py += menuPinballs[i].vy;
        menuPinballs[i].vy += 0.1f;
        if (menuPinballs[i].py > screenHeight + 20) {
            menuPinballs[i].px = 228;
            menuPinballs[i].py = 126;
            menuPinballs[i].vx = ((rand() % 40) / 10.0f) - 2.0f;
            menuPinballs[i].vy = ((rand() % 50) / -10.0f);
        }
    }

    // Handle menu input
    if (inputCenterPressed(input)) {
        // Only start game if NOT on system menu
        if (game->menuState != 2) {
            game->transitionState = 1;
            game->transitionTarget = TRANSITION_TO_GAME;
            playClick(sound);
            //sound_play_haptic_bumper_light(sound);
            inputSendGameStart(input);
        }
    }
    if (inputLeftPressed(input)) {
        playClick(sound);
        //sound_play_haptic_bumper_light(sound);
        game->menuState--;
        if (game->menuState < 0) {
            game->menuState = 2;  // Wrap to controls
        }
        // Reset high score timer when entering high scores scene
        if (game->menuState == 0) {
            game->menuHighScoreTimer = 0;
        }
    }
    if (inputRightPressed(input)) {
        playClick(sound);
        //sound_play_haptic_bumper_light(sound);
        game->menuState++;
        if (game->menuState > 2) {
            game->menuState = 0;  // Wrap to high scores
        }
        // Reset high score timer when entering high scores scene
        if (game->menuState == 0) {
            game->menuHighScoreTimer = 0;
        }
    }
}

void Scoreboard_Update(GameStruct *game,
                       InputManager *input,
                       ScoreHelper *scores,
                       char *nameString) {
    
    // Game over name selection
    if (game->nameSelectDone == 0) {
        if (inputRightPressed(input)) {
            playClick(game->sound);
            game->nameSelectIndex++;
            if (game->nameSelectIndex > 5) {
                game->nameSelectIndex = 0;
            }
        }
        if (inputLeftPressed(input)) {
            playClick(game->sound);
            game->nameSelectIndex--;
            if (game->nameSelectIndex < 0) {
                game->nameSelectIndex = 5;
            }
        }
        if (inputCenterPressed(input)) {
            playClick(game->sound);
            if (game->nameSelectIndex == 5) {
                // Name selection done
                
                // If in Top-3 Game Over scene with camera, capture photo
                if (game->gameState == 3 && game->camera.initialized) {
                    // Trigger NeoPixel camera flash
                    inputSendCameraFlash(input);
                    
                    // Wait for flash (200ms)
                    #if defined(PLATFORM_RPI)
                    usleep(200000);  // 200ms
                    #endif
                    
                    // Capture photo
                    char photoFilename[256];
                    snprintf(photoFilename, sizeof(photoFilename), 
                             "Resources/Photos/%s_%ld.png", nameString, game->gameScore);
                    
                    if (Camera_CapturePhoto(&game->camera, photoFilename)) {
                        printf("Photo captured: %s\n", photoFilename);
                    } else {
                        printf("Failed to capture photo\n");
                    }
                    
                    // Stop camera preview
                    Camera_StopPreview(&game->camera);
                    
                    // Restore normal LED mode
                    inputSendCameraIdle(input);
                }
                
                // Submit score and start transition to menu.
                game->nameSelectDone = 1;
                game->transitionState = 1;
                game->transitionTarget = TRANSITION_TO_MENU;
                submitScore(scores, nameString, game->gameScore);
                printf("Game Over. score: %ld\n", game->gameScore);
                inputSetGameState(input, STATE_MENU);
            } else {
                if (game->nameSelectIndex > 0) {
                    while (game->nameSelectIndex - 1 >= 0 && nameString[game->nameSelectIndex - 1] == 32) {
                        game->nameSelectIndex--;
                    }
                }
                if (nameString[game->nameSelectIndex] < 65 || nameString[game->nameSelectIndex] > 90) {
                    nameString[game->nameSelectIndex] = 65;
                } else {
                    nameString[game->nameSelectIndex] = (nameString[game->nameSelectIndex] + 1);
                    if (nameString[game->nameSelectIndex] > 90) {
                        nameString[game->nameSelectIndex] = 32;
                    } else if (nameString[game->nameSelectIndex] < 65) {
                        nameString[game->nameSelectIndex] = 90;
                    }
                }
            }
        }
    }
}
