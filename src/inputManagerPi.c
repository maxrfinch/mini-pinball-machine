#include "inputManager.h"
#include <stdlib.h>
#include <stdio.h>

static char tempString[64];

InputManager* inputInit(){
    InputManager *input = malloc(sizeof(InputManager));
    input->fd = serialOpen("/dev/ttyACM0",9600);
    input->keyState = 0;
    input->leftKeyPressed = 0;
    input->rightKeyPressed = 0;
    input->centerKeyPressed = 0;
    return input;
}

void inputShutdown(InputManager* input){
    serialClose(input->fd);
}

void inputUpdate(InputManager* input){
    while (serialDataAvail(input->fd) > 0){
        input->keyState = serialGetchar(input->fd);
    }
}

int inputLeft(InputManager* input){
    return (input->keyState & 4);
}

int inputRight(InputManager* input){
    return (input->keyState & 2);
}

int inputCenter(InputManager* input){
    return (input->keyState & 1);
}

int inputLeftPressed(InputManager* input){
    if (inputLeft(input)){
        if (input->leftKeyPressed == 0){
            input->leftKeyPressed = 1;
            // Trigger blink on left button press
            inputSetButtonLED(input, BUTTON_LED_LEFT, LED_MODE_BLINK, 255, 255, 255, 2);
            return 1;
        }
    } else {
        input->leftKeyPressed = 0;
    }
    return 0;
}

int inputRightPressed(InputManager* input){
    if (inputRight(input)){
        if (input->rightKeyPressed == 0){
            input->rightKeyPressed = 1;
            // Trigger blink on right button press
            inputSetButtonLED(input, BUTTON_LED_RIGHT, LED_MODE_BLINK, 255, 255, 255, 2);
            return 1;
        }
    } else {
        input->rightKeyPressed = 0;
    }
    return 0;
}

int inputCenterPressed(InputManager* input){
    if (inputCenter(input)){
        if (input->centerKeyPressed == 0){
            input->centerKeyPressed = 1;
            return 1;
        }
    } else {
        input->centerKeyPressed = 0;
    }
    return 0;
}

// Send game state (new protocol: STATE <id>)
void inputSetGameState(InputManager* input, InputGameState state){
    switch (state){
        case STATE_MENU: {
            sprintf(tempString,"STATE 0\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            // Set menu button LEDs: center breathes, left/right steady
            inputSetButtonLED(input, BUTTON_LED_CENTER, LED_MODE_BREATHE, 0, 255, 255, 0);  // Cyan breathing
            inputSetButtonLED(input, BUTTON_LED_LEFT, LED_MODE_STEADY, 255, 255, 255, 0);   // White steady
            inputSetButtonLED(input, BUTTON_LED_RIGHT, LED_MODE_STEADY, 255, 255, 255, 0);  // White steady
            break;
        }
        case STATE_GAME: {
            sprintf(tempString,"STATE 1\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            // Turn off all button LEDs during gameplay
            inputSetButtonLED(input, BUTTON_LED_CENTER, LED_MODE_OFF, 0, 0, 0, 0);
            inputSetButtonLED(input, BUTTON_LED_LEFT, LED_MODE_OFF, 0, 0, 0, 0);
            inputSetButtonLED(input, BUTTON_LED_RIGHT, LED_MODE_OFF, 0, 0, 0, 0);
            break;
        }
        case STATE_GAME_OVER: {
            sprintf(tempString,"STATE 2\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            // Turn off all button LEDs during game over
            inputSetButtonLED(input, BUTTON_LED_CENTER, LED_MODE_OFF, 0, 0, 0, 0);
            inputSetButtonLED(input, BUTTON_LED_LEFT, LED_MODE_OFF, 0, 0, 0, 0);
            inputSetButtonLED(input, BUTTON_LED_RIGHT, LED_MODE_OFF, 0, 0, 0, 0);
            break;
        }
    }
}
void inputSetScore(InputManager *input, long score){
    sprintf(tempString,"SCORE %ld\n",score);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}
void inputSetNumBalls(InputManager *input, int numBalls){
    sprintf(tempString,"BALLS %d\n",numBalls);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

// Send button LED command: BTN_LED <idx> <mode> <r> <g> <b> [count]
void inputSetButtonLED(InputManager *input, int button_idx, InputLEDMode mode, int r, int g, int b, int count){
    if (count > 0) {
        sprintf(tempString,"BTN_LED %d %d %d %d %d %d\n", button_idx, mode, r, g, b, count);
    } else {
        sprintf(tempString,"BTN_LED %d %d %d %d %d\n", button_idx, mode, r, g, b);
    }
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}
