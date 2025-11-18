#include "ui.h"
#include "constants.h"
#include <stdio.h>
#include <math.h>
#include <sys/time.h>

static long long millis() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

void UI_DrawMenu(const GameStruct *game, const Resources *res,
                 const MenuPinball *menuPinballs, int numMenuPinballs,
                 ScoreHelper *scores, long long elapsedTimeStart,
                 float shaderSeconds) {
    
    ClearBackground((Color){255,183,0,255});
    float timeFactor = (millis() - elapsedTimeStart) / 1000.0f;
    float xOffset = sin(timeFactor) * 50.0f;
    float yOffset = cos(timeFactor) * 50.0f;
    float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
    float width = screenWidth * 3;
    float height = screenHeight * 3;
    BeginShaderMode(res->swirlShader);
    DrawTexturePro(res->bgMenu,(Rectangle){0,0,res->bgMenu.width,res->bgMenu.height},(Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},(Vector2){width/2,height/2},angle,WHITE);
    EndShaderMode();

    // Render pinballs
    for (int i = 0; i < numMenuPinballs; i++){
        DrawTexturePro(res->ballTex,(Rectangle){0,0,res->ballTex.width,res->ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){255,183,0,255});
    }

    DrawTexturePro(res->menuOverlay1,(Rectangle){0,0,res->titleOverlay.width,res->titleOverlay.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);
    DrawTexturePro(res->titleOverlay,(Rectangle){0,0,res->titleOverlay.width,res->titleOverlay.height},(Rectangle){0,12 + sin(timeFactor)*5.0f,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

    if (game->menuState == 0){
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
    } else if (game->menuState == 1){
        DrawTexturePro(res->menuControls,(Rectangle){0,0,res->menuControls.width,res->menuControls.height},(Rectangle){26,320,res->menuControls.width/2,res->menuControls.height/2},(Vector2){0,0},0,WHITE);
    }
}

void UI_DrawGameOver(const GameStruct *game, const Resources *res,
                     const MenuPinball *menuPinballs, int numMenuPinballs,
                     const char *nameString, long long elapsedTimeStart,
                     float shaderSeconds) {
    
    ClearBackground((Color){255,183,0,255});
    float timeFactor = (millis() - elapsedTimeStart) / 1000.0f;
    float xOffset = sin(timeFactor) * 50.0f;
    float yOffset = cos(timeFactor) * 50.0f;
    float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
    float width = screenWidth * 3;
    float height = screenHeight * 3;
    BeginShaderMode(res->swirlShader);
    DrawTexturePro(res->bgMenu,(Rectangle){0,0,res->bgMenu.width,res->bgMenu.height},(Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},(Vector2){width/2,height/2},angle,WHITE);
    EndShaderMode();

    for (int i = 0; i < numMenuPinballs; i++){
        DrawTexturePro(res->ballTex,(Rectangle){0,0,res->ballTex.width,res->ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){0,0,0,50});
    }

    DrawTexturePro(res->gameOverOverlay1,(Rectangle){0,0,res->gameOverOverlay1.width,res->gameOverOverlay1.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);
    DrawTexturePro(res->gameOverOverlay2,(Rectangle){0,0,res->gameOverOverlay2.width,res->gameOverOverlay2.height},(Rectangle){0,12 + sin((millis() - elapsedTimeStart) / 1000.0f)*5.0f,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

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
    DrawTexturePro(res->arrowRight,(Rectangle){0,0,res->arrowRight.width,res->arrowRight.height},(Rectangle){54 + (game->nameSelectIndex * 62),595+ (5 * sin((millis()-elapsedTimeStart)/200.0f)),32,32},(Vector2){16,16},-90,WHITE);
}

void UI_DrawTransition(const GameStruct *game, float shaderSeconds) {
    if (game->transitionState > 0){
        float transitionAmount = ((game->transitionAlpha / 255.0f));
        DrawRectanglePro((Rectangle){screenWidth,screenHeight,screenWidth,screenHeight + 200}, (Vector2){0,screenHeight + 200}, -33.0f * transitionAmount, BLACK);
        DrawRectanglePro((Rectangle){0,0,screenWidth,screenHeight + 200}, (Vector2){screenWidth,0}, -33.0f * transitionAmount, BLACK);
    }
}
