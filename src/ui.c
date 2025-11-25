#include "ui.h"
#include "constants.h"
#include "util.h"
#include "raylib.h"
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

static long long millis_ui() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

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
        // Uses exact 450x800 absolute coordinates derived from 900x1600 design-space SVG

        // Volume circles (radius = 25.0f)
        Vector2 volDownCenter = { 81.38f, 370.0f };
        Vector2 volUpCenter = { 366.87f, 370.0f };
        float volCircleRadius = 25.0f;

        // Draw volume circles
        DrawCircleLines((int)volDownCenter.x, (int)volDownCenter.y, volCircleRadius, WHITE);
        DrawCircleLines((int)volUpCenter.x, (int)volUpCenter.y, volCircleRadius, WHITE);

        // Draw volume down indicator (minus sign)
        DrawLineEx((Vector2){volDownCenter.x - 10.0f, volDownCenter.y}, 
                   (Vector2){volDownCenter.x + 10.0f, volDownCenter.y}, 3.0f, WHITE);
        
        // Draw volume up indicator (plus sign)
        DrawLineEx((Vector2){volUpCenter.x - 10.0f, volUpCenter.y}, 
                   (Vector2){volUpCenter.x + 10.0f, volUpCenter.y}, 3.0f, WHITE);
        DrawLineEx((Vector2){volUpCenter.x, volUpCenter.y - 10.0f}, 
                   (Vector2){volUpCenter.x, volUpCenter.y + 10.0f}, 3.0f, WHITE);

        // Text positions and font sizes (exact coordinates)
        Vector2 volumeTextPos = { 138.52f, 380.24f };
        float volumeFontSize = 27.08f;

        Vector2 tempTextPos = { 160.09f, 432.09f };
        float tempFontSize = 18.75f;

        Vector2 batteryTextPos = { 145.35f, 489.09f };
        float batteryFontSize = 18.75f;

        Vector2 scoresTextPos = { 56.38f, 651.51f };
        float scoresFontSize = 27.08f;

        Vector2 controlsTextPos = { 283.59f, 651.51f };
        float controlsFontSize = 27.08f;

        // Update cached CPU temperature (refreshes each call - lightweight file read)
        game->cachedCpuTemp = readCpuTemperature();

        // Update cached battery percentage (60 second refresh interval)
        long long currentTimeMs = millis_ui();
        if (game->lastBatteryReadTime == 0 || 
            (currentTimeMs - game->lastBatteryReadTime) >= 60000) {
            game->cachedBatteryPercent = readBatteryPercent();
            game->lastBatteryReadTime = currentTimeMs;
        }

        // Draw system info text with live values
        char tempLabel[32];
        char batteryLabel[32];

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

        DrawTextEx(res->font1, "Vol: --", volumeTextPos, volumeFontSize, 1.0f, WHITE);
        DrawTextEx(res->font1, tempLabel, tempTextPos, tempFontSize, 1.0f, WHITE);
        DrawTextEx(res->font1, batteryLabel, batteryTextPos, batteryFontSize, 1.0f, WHITE);

        // Button hitbox rectangles (for drawing outlines - not filled)
        Rectangle shutdownHit = { 56.38f, 527.0f, 156.43f, 67.5f };
        Rectangle desktopHit = { 235.44f, 527.0f, 156.43f, 67.5f };

        // Draw button outlines
        DrawRectangleLinesEx(shutdownHit, 2.0f, WHITE);
        DrawRectangleLinesEx(desktopHit, 2.0f, WHITE);

        // Draw button labels centered in their hitboxes
        const char *shutdownText = "Shutdown";
        Vector2 shutdownTextSize = MeasureTextEx(res->font1, shutdownText, 24.0f, 1.0f);
        DrawTextEx(res->font1, shutdownText, 
                   (Vector2){shutdownHit.x + (shutdownHit.width - shutdownTextSize.x) / 2.0f,
                             shutdownHit.y + (shutdownHit.height - shutdownTextSize.y) / 2.0f},
                   24.0f, 1.0f, WHITE);

        const char *desktopText = "Desktop";
        Vector2 desktopTextSize = MeasureTextEx(res->font1, desktopText, 24.0f, 1.0f);
        DrawTextEx(res->font1, desktopText, 
                   (Vector2){desktopHit.x + (desktopHit.width - desktopTextSize.x) / 2.0f,
                             desktopHit.y + (desktopHit.height - desktopTextSize.y) / 2.0f},
                   24.0f, 1.0f, WHITE);

        // Draw navigation labels (Scores / Controls)
        DrawTextEx(res->font1, "Scores", scoresTextPos, scoresFontSize, 1.0f, WHITE);
        DrawTextEx(res->font1, "Controls", controlsTextPos, controlsFontSize, 1.0f, WHITE);
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
