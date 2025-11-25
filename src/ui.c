#include "ui.h"
#include "constants.h"
#include "util.h"
#include "soundManager.h"
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>

// ============================================================================
// UI Coordinate System
// ============================================================================
// 
// These helper functions convert normalized coordinates (0.0 to 1.0) into
// actual screen pixel coordinates based on screenWidth and screenHeight.
//
// This ensures UI elements scale properly when resolution changes and makes
// it easy to maintain consistent layouts across different screen sizes.
//
// Usage:
//   Rectangle button = UI_Rect(0.5f, 0.5f, 0.2f, 0.1f);
//   // Creates a button at center (50%, 50%) with size (20% width, 10% height)
//
// Reference Design:
//   The original UI was designed for 450×800 resolution. To convert pixel
//   coordinates to normalized values:
//     normalizedX = pixelX / 450.0f
//     normalizedY = pixelY / 800.0f
//
//   Note: Elements stretch proportionally with resolution. For UI elements
//   that must maintain aspect ratio, additional logic would be needed.
// ============================================================================

// UI coordinate helpers - converts normalized positions (0.0-1.0) to screen pixels
// These ensure UI elements scale properly with any screen resolution changes
static inline float UI_X(float normalizedX) {
    return screenWidth * normalizedX;
}

static inline float UI_Y(float normalizedY) {
    return screenHeight * normalizedY;
}

static inline float UI_Width(float normalizedWidth) {
    return screenWidth * normalizedWidth;
}

static inline float UI_Height(float normalizedHeight) {
    return screenHeight * normalizedHeight;
}

// Convenience function for creating rectangles with normalized coordinates
static inline Rectangle UI_Rect(float normX, float normY, float normW, float normH) {
    return (Rectangle){
        UI_X(normX),
        UI_Y(normY),
        UI_Width(normW),
        UI_Height(normH)
    };
}

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

        // Text positions and font sizes (normalized coordinates based on 450×800 design)
        

        Vector2 tempTextPos = { UI_X(0.3558f), UI_Y(0.5401f) };     // 160.09/450, 432.09/800
        float tempFontSize = UI_Height(0.0284f);                     // 22.75/800

        Vector2 batteryTextPos = { UI_X(0.3230f), UI_Y(0.6114f) };  // 145.35/450, 489.09/800
        float batteryFontSize = UI_Height(0.0284f);                  // 22.75/800

        
        Vector2 scoresTextPos = { UI_X(0.1253f), UI_Y(0.8144f) };   // 56.38/450, 651.51/800
        float scoresFontSize = UI_Height(0.0339f);                   // 27.08/800

        Vector2 controlsTextPos = { UI_X(0.6302f), UI_Y(0.8144f) }; // 283.59/450, 651.51/800
        float controlsFontSize = UI_Height(0.0339f);                 // 27.08/800

        // Volume button positions (normalized coordinates based on original 450×800 design)
        // Original values: volButtonY=470, volMinusX=85, volButtonSize=40
        // Note: volPlusX adjusted to 325 (was 445) to keep button fully on-screen with symmetric margins
        Rectangle volMinusRect = UI_Rect(0.1253333333f, 0.43125f, 0.1111111111f, 0.0625f);  // 85/450, 470/800, 40/450, 40/800
        Rectangle volPlusRect = UI_Rect(0.7596666667f, 0.43125f, 0.1111111111f, 0.0625f);   // 325/450, 470/800, 40/450, 40/800

        // Shutdown button position (normalized coordinates)
        // Original: x=85, y=570, w=156, h=67
        Rectangle shutdownRect = UI_Rect(0.1253333333f, 0.65875f, 0.3476666667f, 0.084375f);  // 85/450, 570/800, 156/450, 67/800

        // Quit button position (normalized coordinates)
        // Original: x=255, y=570, w=156, h=67
        Rectangle quitRect = UI_Rect(0.5232222222f, 0.65875f, 0.3476666667f, 0.084375f);      // 255/450, 570/800, 156/450, 67/800

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
        float volCenterX = 0.5f;
        int volumePercent = (int)roundf(sound_getGameVolume(game->sound) * 100.0f);
        sprintf(volumeLabel, "Volume: %d%%", volumePercent);
        float volumeFontSize = UI_Height(0.04f); // 27.08/800 (scaled to height)
        float volTextWidth = MeasureTextEx(res->font1, volumeLabel, volumeFontSize, 1.0).x;
        float halfWidthNormalized = (volTextWidth / 2.0f) / screenWidth;
        Vector2 volumeTextPos = { 
            UI_X(volCenterX - halfWidthNormalized),  // Properly centered
            UI_Y(0.43125f) 
        };   // 138.52/450, 380.24/800                   
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
