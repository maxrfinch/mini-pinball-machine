#include "inputManager.h"
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdio.h>

// Minimal replacements for wiringSerial API using POSIX termios on /dev/ttyACM0

static int serialOpen(const char *device, int baud)
{
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        perror("serialOpen: open failed");
        return fd;
    }

    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        perror("serialOpen: tcgetattr failed");
        close(fd);
        return -1;
    }

    // Set raw mode
    cfmakeraw(&options);

    // Set baud rate (we only need 9600 for now)
    speed_t speed = B9600;
    switch (baud) {
        case 9600:  speed = B9600;  break;
        case 19200: speed = B19200; break;
        case 38400: speed = B38400; break;
        case 57600: speed = B57600; break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;  break;
    }
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    // 8N1, no flow control
    options.c_cflag &= ~PARENB;
    options.c_cflag &= ~CSTOPB;
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag |= CREAD | CLOCAL;

    // Non-canonical, no echo, non-blocking reads
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 0;

    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        perror("serialOpen: tcsetattr failed");
        close(fd);
        return -1;
    }

    return fd;
}

static void serialClose(int fd)
{
    if (fd >= 0) {
        close(fd);
    }
}

static int serialDataAvail(int fd)
{
    int bytes = 0;
    if (ioctl(fd, FIONREAD, &bytes) < 0) {
        return 0;
    }
    return bytes;
}

static int serialGetchar(int fd)
{
    unsigned char ch;
    int n = read(fd, &ch, 1);
    if (n == 1) {
        return (int)ch;
    }
    return -1;
}

static void serialPuts(int fd, const char *s)
{
    if (fd < 0 || s == NULL) return;
    size_t len = strlen(s);
    if (len == 0) return;
    ssize_t written = write(fd, s, len);
    (void)written;
}

static void serialFlush(int fd)
{
    if (fd >= 0) {
        tcflush(fd, TCIOFLUSH);
    }
}

static char tempString[64];
static char receiveBuffer[256];
static int receiveBufferPos = 0;

InputManager* inputInit(){
    InputManager *input = malloc(sizeof(InputManager));
    input->fd = serialOpen("/dev/ttyACM0",9600);
    input->keyState = 0;
    input->leftKeyPressed = 0;
    input->rightKeyPressed = 0;
    input->centerKeyPressed = 0;
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
    receiveBufferPos = 0;
    return input;
}

void inputShutdown(InputManager* input){
    serialClose(input->fd);
}

// Parse button event from KB2040 (e.g., "EVT BUTTON LEFT DOWN")
static void parseButtonEvent(InputManager* input, const char* line) {
    // Expected format: "EVT BUTTON <LEFT|CENTER|RIGHT> <DOWN|UP|HELD>"
    if (strncmp(line, "EVT BUTTON ", 11) != 0) {
        return;
    }
    
    const char* rest = line + 11; // Skip "EVT BUTTON "
    
    // Parse button name
    int button = -1;
    if (strncmp(rest, "LEFT ", 5) == 0) {
        button = 0; // Left = bit 0
        rest += 5;
    } else if (strncmp(rest, "CENTER ", 7) == 0) {
        button = 1; // Center = bit 1
        rest += 7;
    } else if (strncmp(rest, "RIGHT ", 6) == 0) {
        button = 2; // Right = bit 2
        rest += 6;
    } else {
        return; // Unknown button
    }
    
    // Parse state
    int pressed = 0;
    if (strncmp(rest, "DOWN", 4) == 0 || strncmp(rest, "HELD", 4) == 0) {
        pressed = 1;
    } else if (strncmp(rest, "UP", 2) == 0) {
        pressed = 0;
    } else {
        return; // Unknown state
    }
    
    // Update keyState bit flags
    int bitMask = (1 << button);
    if (pressed) {
        input->keyState |= bitMask;
    } else {
        input->keyState &= ~bitMask;
    }
    
    fprintf(stderr, "DBG Pi parsed button event: button=%d pressed=%d keyState=0x%02x\n", 
            button, pressed, input->keyState);
}

void inputUpdate(InputManager* input){
    // Read available characters and parse button events
    while (serialDataAvail(input->fd) > 0){
        int ch = serialGetchar(input->fd);
        if (ch < 0) break;
        
        // Add to buffer
        if (ch == '\n' || ch == '\r') {
            // End of line - process the buffer
            if (receiveBufferPos > 0) {
                receiveBuffer[receiveBufferPos] = '\0';
                parseButtonEvent(input, receiveBuffer);
                receiveBufferPos = 0;
            }
        } else if (receiveBufferPos < sizeof(receiveBuffer) - 1) {
            receiveBuffer[receiveBufferPos++] = (char)ch;
        } else {
            // Buffer overflow - reset
            receiveBufferPos = 0;
        }
    }
}

int inputLeft(InputManager* input){
    // Physical left button corresponds to bit 0 (0x01)
    return (input->keyState & 1);
}

int inputRight(InputManager* input){
    // Physical right button corresponds to bit 2 (0x04)
    return (input->keyState & 4);
}

int inputCenter(InputManager* input){
    // Physical center button corresponds to bit 1 (0x02)
    return (input->keyState & 2);
}


int inputLeftPressed(InputManager* input){
    if (inputLeft(input)){
        if (input->leftKeyPressed == 0){
            input->leftKeyPressed = 1;
            // Pico handles LED animation on button press
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
            // Pico handles LED animation on button press
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

// Send game state - Pi-centric architecture: Pi manages state, sends explicit effect commands
void inputSetGameState(InputManager* input, InputGameState state){
    switch (state){
        case STATE_MENU: {
            // Menu state: show menu navigation visuals
            sprintf(tempString,"CMD NEO EFFECT ATTRACT\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            sprintf(tempString,"CMD BUTTON EFFECT ALL MENU_NAVIGATION\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            break;
        }
        case STATE_GAME: {
            // Game state: set to ball-ready visuals
            // Ball launch effect with center button pulse
            sprintf(tempString,"CMD NEO EFFECT BALL_LAUNCH\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            sprintf(tempString,"CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            break;
        }
        case STATE_GAME_OVER: {
            // Game over state: pink pulse and fade
            sprintf(tempString,"CMD NEO EFFECT PINK_PULSE\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            sprintf(tempString,"CMD BUTTON EFFECT ALL GAME_OVER_FADE\n");
            serialPuts(input->fd,tempString);
            serialFlush(input->fd);
            break;
        }
    }
}

void inputSetScore(InputManager *input, long score){
    sprintf(tempString,"CMD DISPLAY SCORE %ld\n",score);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

void inputSetNumBalls(InputManager *input, int numBalls){
    sprintf(tempString,"CMD DISPLAY BALLS %d\n",numBalls);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

// Send button LED command - now uses CMD BUTTON EFFECT syntax
// Pi-centric: Pi sends explicit effect commands
void inputSetButtonLED(InputManager *input, int button_idx, InputLEDMode mode, int r, int g, int b, int count){
    // Map old LED mode to new button effects
    // This provides backwards compatibility for existing game code
    const char* button_name;
    const char* effect_name;
    
    switch (button_idx) {
        case 0: button_name = "LEFT"; break;
        case 1: button_name = "CENTER"; break;
        case 2: button_name = "RIGHT"; break;
        default: button_name = "ALL"; break;
    }
    
    // Map mode to effect (simplified mapping)
    switch (mode) {
        case LED_MODE_STEADY:
            effect_name = "READY_STEADY_GLOW";
            break;
        case LED_MODE_STROBE:
            effect_name = "POWERUP_ALERT";
            break;
        default:
            effect_name = "READY_STEADY_GLOW";
            break;
    }
    
    sprintf(tempString,"CMD BUTTON EFFECT %s %s\n", button_name, effect_name);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

// Send game event using CMD EVENT syntax
void inputSendEvent(InputManager *input, const char *event_name){
    sprintf(tempString,"CMD EVENT %s\n", event_name);
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

// Convenience functions for common events
void inputSendGameStart(InputManager *input){
    // Game start: transition to ball-ready visuals
    sprintf(tempString,"CMD NEO EFFECT BALL_LAUNCH\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
    sprintf(tempString,"CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

void inputSendBallReady(InputManager *input){
    // Ball ready: center button pulse
    sprintf(tempString,"CMD NEO EFFECT BALL_LAUNCH\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
    sprintf(tempString,"CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

void inputSendBallLaunched(InputManager *input){
    // Ball launched: transition to in-play visuals
    sprintf(tempString,"CMD NEO EFFECT NONE\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
    sprintf(tempString,"CMD BUTTON EFFECT ALL READY_STEADY_GLOW\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

void inputSendBallSavedAnimation(InputManager *input){
    // Trigger ball saved display animation
    sprintf(tempString,"CMD DISPLAY BALL_SAVED\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}

void inputSendMultiballAnimation(InputManager *input){
    // Trigger multiball display animation
    sprintf(tempString,"CMD DISPLAY MULTIBALL\n");
    serialPuts(input->fd,tempString);
    serialFlush(input->fd);
}
