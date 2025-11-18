#ifndef GAME_H
#define GAME_H

#include "gameStruct.h"
#include "inputManager.h"
#include "soundManager.h"
#include "scores.h"

// Initialize game state machine
void Game_Init(GameStruct *game, Bumper *bumpers);

// Update game state machine and handle transitions
void Game_Update(GameStruct *game,
                 Bumper *bumpers,
                 InputManager *input,
                 ScoreHelper *scores,
                 SoundManager *sound,
                 float dt);

// Start a new game
void Game_StartGame(GameStruct *game, Bumper *bumpers);

#endif // GAME_H
