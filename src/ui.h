#ifndef UI_H
#define UI_H

#include "raylib.h"
#include "resources.h"
#include "gameStruct.h"
#include "scores.h"

#ifdef __cplusplus
extern "C" {
#endif

// Draws menu / attract mode overlays when in a menu state
void UI_DrawMenu(const GameStruct *game, const Resources *res,
                 const MenuPinball *menuPinballs, int numMenuPinballs,
                 ScoreHelper *scores, long long elapsedTimeStart,
                 float shaderSeconds);

// Draws score / high score screen overlays (game over screen)
void UI_DrawGameOver(const GameStruct *game, const Resources *res,
                     const MenuPinball *menuPinballs, int numMenuPinballs,
                     const char *nameString, long long elapsedTimeStart,
                     float shaderSeconds);

// Draws transition overlays (screen wipes)
void UI_DrawTransition(const GameStruct *game, float shaderSeconds);

#ifdef __cplusplus
}
#endif

#endif // UI_H
