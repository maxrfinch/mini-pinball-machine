#ifndef MENU_H
#define MENU_H

#include "gameStruct.h"
#include "inputManager.h"
#include "soundManager.h"
#include "scores.h"

// Initialize menu system
void Menu_Init(GameStruct *game, MenuPinball *menuPinballs, int numMenuPinballs);

// Update menu state and pinballs
void Menu_Update(GameStruct *game,
                 MenuPinball *menuPinballs,
                 int numMenuPinballs,
                 InputManager *input,
                 SoundManager *sound);

// Update scoreboard cycling
void Scoreboard_Update(GameStruct *game,
                       InputManager *input,
                       ScoreHelper *scores,
                       char *nameString);

#endif // MENU_H
