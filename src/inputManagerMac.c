#include "raylib.h"
#include "inputManager.h"
#include <stdlib.h>

InputManager* inputInit(){
    InputManager *input = malloc(sizeof(InputManager));
    return input;
}

void inputUpdate(InputManager* input){
    return;
}
void inputShutdown(InputManager* input){
    return;
}

int inputLeft(InputManager* input){
    return IsKeyDown(KEY_LEFT) || GetGamepadAxisMovement(0, 4) > -0.75;
}

int inputRight(InputManager* input){
    return IsKeyDown(KEY_RIGHT) || GetGamepadAxisMovement(0, 5) > -0.75;
}

int inputCenter(InputManager* input){
    return IsKeyDown(KEY_SPACE) || IsGamepadButtonDown(0, 7);
}

int inputLeftPressed(InputManager* input){
    return IsKeyPressed(KEY_LEFT);
}

int inputRightPressed(InputManager* input){
    return IsKeyPressed(KEY_RIGHT);
}

int inputCenterPressed(InputManager* input){
    return IsKeyPressed(KEY_SPACE) || IsGamepadButtonPressed(0, 7);
}

void inputSetGameState(InputManager* input, InputGameState state){

}
void inputSetScore(InputManager *input, long score){

}
void inputSetNumBalls(InputManager *input, int numBalls){
    
}

void inputSetButtonLED(InputManager *input, int button_idx, InputLEDMode mode, int r, int g, int b, int count){
    // Stub for Mac - no hardware button LEDs
    (void)input;
    (void)button_idx;
    (void)mode;
    (void)r;
    (void)g;
    (void)b;
    (void)count;
}

void inputSendEvent(InputManager *input, const char *event_name){
    // Stub for Mac - no hardware controller
    (void)input;
    (void)event_name;
}

void inputSendGameStart(InputManager *input){
    // Stub for Mac - no hardware controller
    (void)input;
}

void inputSendBallReady(InputManager *input){
    // Stub for Mac - no hardware controller
    (void)input;
}

void inputSendBallLaunched(InputManager *input){
    // Stub for Mac - no hardware controller
    (void)input;
}

void inputSendBallSavedAnimation(InputManager *input){
    // Stub for Mac - no hardware controller
    (void)input;
}

void inputSendMultiballAnimation(InputManager *input){
    // Stub for Mac - no hardware controller
    (void)input;
}
