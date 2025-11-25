#include "ui.h"
#include "constants.h"
#include "util.h"
#include "soundManager.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

static long long millis_ui() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

// Round a float to the nearest 0.1 (10% increment)
static float roundToTenth(float value) {
    return roundf(value * 10.0f) / 10.0f;
}

// Debug UI toggle - press Tab to show button hitboxes
static int debugUIEnabled = 0;

void UI_DrawMenu(GameStruct *game, const Resources *res,
                 const MenuPinball *menuPinballs, int numMenuPinballs,
                 ScoreHelper *scores, long long elapsedTimeStart,
                 float shaderSeconds) {
    
    ClearBackground((Color){255,183,0,255});
    float timeFactor = (millis_ui() - elapsedTimeStart) / 1000.0f;
    float xOffset = sin(timeFactor) * 50.0f;
    float yOffset = cos(timeFactor) * 50.0f;
    float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
    float width = screenWidth * 3;
    float height = screenHeight * 3;

    bool useSwirl = IsShaderValid(res->swirlShader);
#if defined(PLATFORM_RPI)
    // On Raspberry Pi with DRM/GLES, swirl shader can be problematic; disable if needed.
    useSwirl = true;
#endif

    if (useSwirl) {
        BeginShaderMode(res->swirlShader);
    }

    DrawTexturePro(res->bgMenu,
                   (Rectangle){0,0,res->bgMenu.width,res->bgMenu.height},
                   (Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},
                   (Vector2){width/2,height/2},
                   angle,
                   WHITE);

    if (useSwirl) {
        EndShaderMode();
    }

    // Render pinballs
    for (int i = 0; i < numMenuPinballs; i++){
        DrawTexturePro(res->ballTex,(Rectangle){0,0,res->ballTex.width,res->ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){255,183,0,255});
    }

    // Determine which overlay to draw based on menuState and timer
    Texture2D currentOverlay;
    int showTopThree = 0;

    if (game->menuState == 0) {
        // High scores scene - cycle between all-time (10s) and top 3 (10s)
        long long currentTime = millis_ui();
        if (game->menuHighScoreTimer == 0) {
            game->menuHighScoreTimer = currentTime;
        }
        
        long long elapsed = currentTime - game->menuHighScoreTimer;
        int cyclePosition = (elapsed / 10000) % 2;  // 10 second intervals, 0 or 1
        
        if (cyclePosition == 0) {
            currentOverlay = res->menuOverlayHighscores;
            showTopThree = 0;
        } else {
            currentOverlay = res->menuOverlayTopScores;
            showTopThree = 1;
        }
    } else if (game->menuState == 1) {
        currentOverlay = res->menuOverlayControls;
    } else {  // menuState == 2
        currentOverlay = res->menuOverlaySystem;
    }

    // Draw the appropriate overlay
    DrawTexturePro(currentOverlay,
                   (Rectangle){0,0,currentOverlay.width,currentOverlay.height},
                   (Rectangle){0,0,screenWidth,screenHeight},
                   (Vector2){0,0},0,WHITE);

    DrawTexturePro(res->titleOverlay,
                   (Rectangle){0,0,res->titleOverlay.width,res->titleOverlay.height},
                   (Rectangle){0,12 + sin(timeFactor)*5.0f,screenWidth,screenHeight},
                   (Vector2){0,0},0,WHITE);

    // Render content based on menu state
    if (game->menuState == 0) {
        // High scores - show either top 10 or top 3
        if (showTopThree) {
            DrawTextEx(res->font1, "Top 3", (Vector2){screenWidth/2 - MeasureTextEx(res->font1, "Top 3", 36.0, 1.0).x/2, 329}, 36.0, 1.0, WHITE);
            float y = 362;
            char tempString[128];
            for (int i = 1; i <= 3; i++) {
                ScoreObject *score = getRankedScore(scores, i);
                if (score != NULL) {
                    sprintf(tempString,"%d)",i);
                    DrawTextEx(res->font1, tempString, (Vector2){66 - MeasureTextEx(res->font1, tempString, 27.0, 1.0).x,y}, 27.0, 1.0, WHITE);
                    sprintf(tempString,"%s",score->scoreName);
                    DrawTextEx(res->font1, tempString, (Vector2){75,y}, 27.0, 1.0, WHITE);
                    float scoreNameWidth = MeasureTextEx(res->font1, tempString, 27.0, 1.0).x;
                    sprintf(tempString,"%d",score->scoreValue);
                    float scoreValueWidth = MeasureTextEx(res->font1, tempString, 27.0, 1.0).x;
                    DrawTextEx(res->font1, tempString, (Vector2){404 - scoreValueWidth,y}, 27.0, 1.0, WHITE);
                    float lineY = y + 27.0 / 2.0f - 1.0f;
                    DrawLineEx((Vector2){75 + (scoreNameWidth + 10),lineY}, (Vector2){404 - (scoreValueWidth + 10),lineY}, 2, (Color){255,255,255,50});
                }
                y += (27.0 * 1.5) + 5;  // Larger spacing for top 3
            }
        } else {
            // Existing top 10 rendering code
            DrawTextEx(res->font1, "Top Scores", (Vector2){153,329}, 36.0, 1.0, WHITE);
            float y = 362;
            char tempString[128];
            for (int i = 1; i <= 10; i++){
                ScoreObject *score = getRankedScore(scores,i);
                if (score != NULL){
                    sprintf(tempString,"%d)",i);
                    DrawTextEx(res->font1, tempString, (Vector2){66 - MeasureTextEx(res->font1, tempString, 27.0, 1.0).x,y}, 27.0, 1.0, WHITE);
                    sprintf(tempString,"%s",score->scoreName);
                    DrawTextEx(res->font1, tempString, (Vector2){75,y}, 27.0, 1.0, WHITE);
                    float scoreNameWidth = MeasureTextEx(res->font1, tempString, 27.0, 1.0).x;
                    sprintf(tempString,"%d",score->scoreValue);
                    float scoreValueWidth = MeasureTextEx(res->font1, tempString, 27.0, 1.0).x;
                    DrawTextEx(res->font1, tempString, (Vector2){404 - scoreValueWidth,y}, 27.0, 1.0, WHITE);
                    float lineY = y + 27.0 / 2.0f - 1.0f;
                    DrawLineEx((Vector2){75 + (scoreNameWidth + 10),lineY}, (Vector2){404 - (scoreValueWidth + 10),lineY}, 2, (Color){255,255,255,50});
                } else {
                    sprintf(tempString,"%d)",i);
                    DrawTextEx(res->font1, tempString, (Vector2){66 - MeasureTextEx(res->font1, tempString, 27.0, 1.0).x,y}, 27.0, 1.0, GRAY);
                    DrawTextEx(res->font1, "No Score", (Vector2){75,y}, 27.0, 1.0, GRAY);
                }
                y += (27.0 * 0.8) + 2;
            }
        }
    } else if (game->menuState == 1) {
        // Controls - keep existing rendering
        DrawTexturePro(res->menuControls,
                       (Rectangle){0,0,res->menuControls.width,res->menuControls.height},
                       (Rectangle){26,320,res->menuControls.width/2,res->menuControls.height/2},
                       (Vector2){0,0},0,WHITE);
    } else if (game->menuState == 2) {
        // System menu - display system info and buttons

        // Toggle debug UI drawing with Tab key
        if (IsKeyPressed(KEY_TAB)) {
            debugUIEnabled = !debugUIEnabled;
        }

        // Text positions and font sizes (exact coordinates)
        Vector2 volumeTextPos = { 138.52f, 380.24f };
        float volumeFontSize = 27.08f;

        Vector2 tempTextPos = { 160.09f, 432.09f };
        float tempFontSize = 22.75f;

        Vector2 batteryTextPos = { 145.35f, 489.09f };
        float batteryFontSize = 22.75f;

        
        Vector2 scoresTextPos = { 56.38f, 651.51f };
        float scoresFontSize = 27.08f;

        Vector2 controlsTextPos = { 283.59f, 651.51f };
        float controlsFontSize = 27.08f;

        // Volume button hitboxes (transparent overlays over existing texture buttons)
        /*
        float volButtonY = 370.0f;
        float volMinusX = 60.0f;
        float volPlusX = 380.0f;
        */
        float volButtonSize = 40.0f;
        float volButtonY = 470.0f;
        float volMinusX = 85.0f;
        float volPlusX = 445.0f;
        Rectangle volMinusRect = { volMinusX, volButtonY, volButtonSize, volButtonSize };
        Rectangle volPlusRect = { volPlusX, volButtonY, volButtonSize, volButtonSize };

        // Shutdown and Quit button hitboxes (transparent overlays over existing texture buttons)
        // These buttons are rendered as part of menuOverlaySystem texture
        float shutdownButtonX = 85.0f;
        float shutdownButtonY = 570.0f;
        float shutdownButtonW = 156.0f;
        float shutdownButtonH = 67.0f;
        Rectangle shutdownRect = { shutdownButtonX, shutdownButtonY, shutdownButtonW, shutdownButtonH };

        float quitButtonX = 255.0f;
        float quitButtonY = 570.0f;
        float quitButtonW = 156.0f;
        float quitButtonH = 67.0f;
        Rectangle quitRect = { quitButtonX, quitButtonY, quitButtonW, quitButtonH };

        // Update cached CPU temperature (10 second refresh interval)
        long long currentTimeMs = millis_ui();
        if (game->lastTempReadTime == 0 || 
            (currentTimeMs - game->lastTempReadTime) >= 10000) {
            game->cachedCpuTemp = readCpuTemperature();
            game->lastTempReadTime = currentTimeMs;
        }

        // Update cached battery percentage (60 second refresh interval)
        if (game->lastBatteryReadTime == 0 || 
            (currentTimeMs - game->lastBatteryReadTime) >= 60000) {
            game->cachedBatteryPercent = readBatteryPercent();
            game->lastBatteryReadTime = currentTimeMs;
        }

        // Handle touchscreen button input (transparent overlays)
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mousePos = GetMousePosition();
            
            if (CheckCollisionPointRec(mousePos, volMinusRect)) {
                // Volume down by 10%
                float currentVolume = sound_getGameVolume(game->sound);
                float newVolume = roundToTenth(currentVolume - 0.1f);
                if (newVolume < 0.0f) {
                    newVolume = 0.0f;
                }
                sound_setGameVolume(game->sound, newVolume);
                playClick(game->sound);
            } else if (CheckCollisionPointRec(mousePos, volPlusRect)) {
                // Volume up by 10%
                float currentVolume = sound_getGameVolume(game->sound);
                float newVolume = roundToTenth(currentVolume + 0.1f);
                if (newVolume > 1.0f) {
                    newVolume = 1.0f;
                }
                sound_setGameVolume(game->sound, newVolume);
                playClick(game->sound);
            } else if (CheckCollisionPointRec(mousePos, shutdownRect)) {
                // Shutdown the Raspberry Pi
                // Note: Requires passwordless sudo for shutdown command
                // Configure with: echo "pi ALL=(ALL) NOPASSWD: /sbin/shutdown" | sudo tee /etc/sudoers.d/shutdown
                playClick(game->sound);
                #if defined(PLATFORM_RPI) || defined(PLATFORM_LINUX)
                int result = system("sudo shutdown -h now");
                if (result != 0) {
                    // Shutdown command failed - continue running
                    printf("Warning: shutdown command failed with code %d\n", result);
                }
                #endif
            } else if (CheckCollisionPointRec(mousePos, quitRect)) {
                // Quit to desktop
                playClick(game->sound);
                game->quitRequested = 1;
            }
        }

        // Draw system info text with live values
        char volumeLabel[32];
        char tempLabel[32];
        char batteryLabel[32];

        // Display current volume percentage
        int volumePercent = (int)roundf(sound_getGameVolume(game->sound) * 100.0f);
        sprintf(volumeLabel, "Volume: %d%%", volumePercent);
        DrawTextEx(res->font1, volumeLabel, volumeTextPos, volumeFontSize, 1.0f, WHITE);

        if (game->cachedCpuTemp >= 0) {
            sprintf(tempLabel, "Sys Temp: %dC", game->cachedCpuTemp);
        } else {
            sprintf(tempLabel, "Sys Temp: --C");
        }

        if (game->cachedBatteryPercent >= 0) {
            sprintf(batteryLabel, "Battery Life: %d%%", game->cachedBatteryPercent);
        } else {
            sprintf(batteryLabel, "Battery Life: --%%");
        }

        DrawTextEx(res->font1, tempLabel, tempTextPos, tempFontSize, 1.0f, WHITE);
        DrawTextEx(res->font1, batteryLabel, batteryTextPos, batteryFontSize, 1.0f, WHITE);

        // Debug UI: Draw button hitboxes when Tab is pressed
        if (debugUIEnabled) {
            // Draw volume button hitboxes
            DrawRectangleLinesEx(volMinusRect, 2, RED);
            DrawTextEx(res->font1, "VOL-", (Vector2){volMinusRect.x + 2, volMinusRect.y + 2}, 12.0f, 1.0f, RED);
            
            DrawRectangleLinesEx(volPlusRect, 2, RED);
            DrawTextEx(res->font1, "VOL+", (Vector2){volPlusRect.x + 2, volPlusRect.y + 2}, 12.0f, 1.0f, RED);
            
            // Draw shutdown button hitbox
            DrawRectangleLinesEx(shutdownRect, 2, ORANGE);
            DrawTextEx(res->font1, "SHUTDOWN", (Vector2){shutdownRect.x + 2, shutdownRect.y + 2}, 12.0f, 1.0f, ORANGE);
            
            // Draw quit button hitbox
            DrawRectangleLinesEx(quitRect, 2, GREEN);
            DrawTextEx(res->font1, "QUIT", (Vector2){quitRect.x + 2, quitRect.y + 2}, 12.0f, 1.0f, GREEN);
        }
        
    }
}

void UI_DrawGameOver(const GameStruct *game, const Resources *res,
                     const MenuPinball *menuPinballs, int numMenuPinballs,
                     const char *nameString, long long elapsedTimeStart,
                     float shaderSeconds) {
    
    ClearBackground((Color){255,183,0,255});
    float timeFactor = (millis_ui() - elapsedTimeStart) / 1000.0f;
    float xOffset = sin(timeFactor) * 50.0f;
    float yOffset = cos(timeFactor) * 50.0f;
    float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
    float width = screenWidth * 3;
    float height = screenHeight * 3;

    bool useSwirl = IsShaderValid(res->swirlShader);
#if defined(PLATFORM_RPI)
    // On Raspberry Pi with DRM/GLES, swirl shader can be problematic; disable if needed.
    useSwirl = false;
#endif

    if (useSwirl) {
        BeginShaderMode(res->swirlShader);
    }

    DrawTexturePro(res->bgMenu,
                   (Rectangle){0,0,res->bgMenu.width,res->bgMenu.height},
                   (Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},
                   (Vector2){width/2,height/2},
                   angle,
                   WHITE);

    if (useSwirl) {
        EndShaderMode();
    }

    for (int i = 0; i < numMenuPinballs; i++){
        DrawTexturePro(res->ballTex,(Rectangle){0,0,res->ballTex.width,res->ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){0,0,0,50});
    }

    DrawTexturePro(res->gameOverOverlay1,(Rectangle){0,0,res->gameOverOverlay1.width,res->gameOverOverlay1.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);
    DrawTexturePro(res->gameOverOverlay2,(Rectangle){0,0,res->gameOverOverlay2.width,res->gameOverOverlay2.height},(Rectangle){0,12 + sin((millis_ui() - elapsedTimeStart) / 1000.0f)*5.0f,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

    char tempString[128];
    sprintf(tempString,"%ld",game->gameScore);
    DrawTextEx(res->font2, "Score:", (Vector2){screenWidth/2 - MeasureTextEx(res->font2, "Score:", 60, 1.0).x/2,275}, 60, 1.0, WHITE);
    DrawTextEx(res->font2, tempString, (Vector2){screenWidth/2 - MeasureTextEx(res->font2, tempString, 60, 1.0).x/2,332}, 60, 1.0, WHITE);

    for (int i =0; i < 5; i++){
        sprintf(tempString,"%c",nameString[i]);
        float textWidth = MeasureTextEx(res->font2, tempString, 60, 1.0).x;
        if (nameString[i] == 32){
            DrawTextEx(res->font2, "-", (Vector2){54 + (i * 62) - textWidth / 2,510}, 60, 1.0, DARKGRAY);
        } else {
            DrawTextEx(res->font2, tempString, (Vector2){54 + (i * 62) - textWidth / 2,510}, 60, 1.0, WHITE);
        }
    }
    DrawTexturePro(res->arrowRight,(Rectangle){0,0,res->arrowRight.width,res->arrowRight.height},(Rectangle){54 + (game->nameSelectIndex * 62),595+ (5 * sin((millis_ui()-elapsedTimeStart)/200.0f)),32,32},(Vector2){16,16},-90,WHITE);
}

void UI_DrawTransition(const GameStruct *game, float shaderSeconds) {
    if (game->transitionState > 0){
        float transitionAmount = ((game->transitionAlpha / 255.0f));
        DrawRectanglePro((Rectangle){screenWidth,screenHeight,screenWidth,screenHeight + 200}, (Vector2){0,screenHeight + 200}, -33.0f * transitionAmount, BLACK);
        DrawRectanglePro((Rectangle){0,0,screenWidth,screenHeight + 200}, (Vector2){screenWidth,0}, -33.0f * transitionAmount, BLACK);
    }
}
