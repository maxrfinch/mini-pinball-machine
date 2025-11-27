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

// Force write buffer to be transmitted to device
static void serialDrain(int fd)
{
    if (fd >= 0) {
        tcdrain(fd);  // Wait until all output written to fd has been transmitted
    }
}

// Flush/discard input/output buffers (use sparingly)
static void serialFlush(int fd)
{
    if (fd >= 0) {
        tcflush(fd, TCIOFLUSH);
    }
}

// Configuration constants
#define TEMP_STRING_SIZE 128       // Command buffer size (must fit longest command)
#define RECEIVE_BUFFER_SIZE 256    // Input event buffer size
#define INTER_BATCH_DELAY_US 5000  // 5ms delay between command batches to prevent KB2040 buffer saturation

static char tempString[TEMP_STRING_SIZE];
static char receiveBuffer[RECEIVE_BUFFER_SIZE];
static int receiveBufferPos = 0;

// Shared buffers for command batching to avoid stack allocation
static char batchCmd1[TEMP_STRING_SIZE];
static char batchCmd2[TEMP_STRING_SIZE];

// Helper function to send a command with proper flushing
static void sendCommand(int fd, const char *cmd)
{
    if (fd < 0 || cmd == NULL) return;
    serialPuts(fd, cmd);
    serialDrain(fd);  // Ensure command is transmitted before returning
}

// Helper function to send multiple commands with proper sequencing
static void sendCommandBatch(int fd, const char **commands, int count)
{
    if (fd < 0 || commands == NULL || count <= 0) return;
    
    for (int i = 0; i < count; i++) {
        if (commands[i] != NULL) {
            serialPuts(fd, commands[i]);
        }
    }
    
    // Drain once after all commands written to buffer
    serialDrain(fd);
    
    // Small delay to allow KB2040 to process before next batch
    usleep(INTER_BATCH_DELAY_US);
}

// Helper to send a 2-command batch (common case)
static void sendCommand2(int fd, const char *cmd1, const char *cmd2)
{
    const char *commands[2] = {cmd1, cmd2};
    sendCommandBatch(fd, commands, 2);
}

// Helper to send a 3-command batch
static void sendCommand3(int fd, const char *cmd1, const char *cmd2, const char *cmd3)
{
    const char *commands[3] = {cmd1, cmd2, cmd3};
    sendCommandBatch(fd, commands, 3);
}

// Shared buffer for shutdown command
static char batchCmd3[TEMP_STRING_SIZE];

InputManager* inputInit(){
    InputManager *input = malloc(sizeof(InputManager));
    input->fd = serialOpen("/dev/ttyACM0",9600);
    input->keyState = 0;
    input->leftKeyPressed = 0;
    input->rightKeyPressed = 0;
    input->centerKeyPressed = 0;
    memset(receiveBuffer, 0, sizeof(receiveBuffer));
    receiveBufferPos = 0;
    
    // Set NeoPixel brightness to 80 out of 255 at startup
    sprintf(tempString, "CMD NEO BRIGHTNESS 65\n");
    sendCommand(input->fd, tempString);
    
    return input;
}

void inputShutdownEffects(InputManager* input){
    if (input == NULL || input->fd < 0) {
        return;
    }
    
    // Turn off all NeoPixel effects
    sprintf(batchCmd1, "CMD NEO EFFECT NONE\n");
    // Turn off all button LED effects
    sprintf(batchCmd2, "CMD BUTTON EFFECT ALL OFF\n");
    // Clear display
    sprintf(batchCmd3, "CMD DISPLAY CLEAR\n");
    
    sendCommand3(input->fd, batchCmd1, batchCmd2, batchCmd3);
    
    // Small delay to ensure commands are sent
    usleep(100000); // 100ms
}

void inputShutdown(InputManager* input){
    if (input != NULL) {
        inputShutdownEffects(input);
        serialClose(input->fd);
        free(input);
    }
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
        } else if (receiveBufferPos < RECEIVE_BUFFER_SIZE - 1) {
            receiveBuffer[receiveBufferPos++] = (char)ch;
        } else {
            // Buffer overflow - reset and log warning
            fprintf(stderr, "WARN: Input buffer overflow, discarding incomplete message\n");
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
            sprintf(batchCmd1, "CMD NEO EFFECT ATTRACT\n");
            sprintf(batchCmd2, "CMD BUTTON EFFECT ALL MENU_NAVIGATION\n");
            sendCommand2(input->fd, batchCmd1, batchCmd2);
            break;
        }
        case STATE_GAME: {
            // Game state: set to ball-ready visuals
            // Ball launch effect with center button pulse
            sprintf(batchCmd1, "CMD NEO EFFECT BALL_LAUNCH\n");
            sprintf(batchCmd2, "CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
            sendCommand2(input->fd, batchCmd1, batchCmd2);
            break;
        }
        case STATE_GAME_OVER: {
            // Game over state: pink pulse and fade
            sprintf(batchCmd1, "CMD NEO EFFECT PINK_PULSE\n");
            sprintf(batchCmd2, "CMD BUTTON EFFECT ALL GAME_OVER_FADE\n");
            sendCommand2(input->fd, batchCmd1, batchCmd2);
            break;
        }
    }
}

void inputSetScore(InputManager *input, long score){
    sprintf(tempString,"CMD DISPLAY SCORE %ld\n",score);
    sendCommand(input->fd, tempString);
}

void inputSetNumBalls(InputManager *input, int numBalls){
    sprintf(tempString,"CMD DISPLAY BALLS %d\n",numBalls);
    sendCommand(input->fd, tempString);
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
    sendCommand(input->fd, tempString);
}

// Send game event using CMD EVENT syntax
void inputSendEvent(InputManager *input, const char *event_name){
    sprintf(tempString,"CMD EVENT %s\n", event_name);
    sendCommand(input->fd, tempString);
}

// Convenience functions for common events
void inputSendGameStart(InputManager *input){
    // Game start: transition to ball-ready visuals
    sprintf(batchCmd1, "CMD NEO EFFECT BALL_LAUNCH\n");
    sprintf(batchCmd2, "CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
    sendCommand2(input->fd, batchCmd1, batchCmd2);
}

void inputSendBallReady(InputManager *input){
    // Ball ready: center button pulse
    sprintf(batchCmd1, "CMD NEO EFFECT BALL_LAUNCH\n");
    sprintf(batchCmd2, "CMD BUTTON EFFECT CENTER CENTER_HIT_PULSE\n");
    sendCommand2(input->fd, batchCmd1, batchCmd2);
}

void inputSendBallLaunched(InputManager *input){
    // Ball launched: transition to in-play visuals
    sprintf(batchCmd1, "CMD NEO EFFECT RAINBOW_BREATHE\n");
    sprintf(batchCmd2, "CMD BUTTON EFFECT ALL READY_STEADY_GLOW\n");
    sendCommand2(input->fd, batchCmd1, batchCmd2);
}

void inputSendBallSavedAnimation(InputManager *input){
    // Trigger ball saved display animation
    sprintf(tempString,"CMD DISPLAY BALL_SAVED\n");
    sendCommand(input->fd, tempString);
}

void inputSendMultiballAnimation(InputManager *input){
    // Trigger multiball display animation
    sprintf(tempString,"CMD DISPLAY MULTIBALL\n");
    sendCommand(input->fd, tempString);
}

void inputSendCameraPreview(InputManager *input){
    // Trigger camera preview NeoPixel mode
    // Note: CAMERA_PREVIEW is not defined in firmware, using NONE for minimal distraction
    sprintf(tempString,"CMD NEO EFFECT NONE\n");
    sendCommand(input->fd, tempString);
}

void inputSendCameraFlash(InputManager *input){
    // Trigger camera flash NeoPixel effect
    sprintf(tempString,"CMD NEO EFFECT CAMERA_FLASH\n");
    sendCommand(input->fd, tempString);
}

void inputSendCameraIdle(InputManager *input){
    // Return to idle/attract mode after camera
    sprintf(tempString,"CMD NEO EFFECT ATTRACT\n");
    sendCommand(input->fd, tempString);
}

void inputSendWaterEffectStart(InputManager *input){
    // Start water NeoPixel and matrix effect
    sprintf(batchCmd1, "CMD NEO EFFECT WATER\n");
    sprintf(batchCmd2, "CMD DISP_EFFECT WATER_RIPPLE\n");
    sendCommand2(input->fd, batchCmd1, batchCmd2);
}

void inputSendWaterEffectEnd(InputManager *input){
    // End water effects, return to in-game visuals
    sprintf(batchCmd1, "CMD NEO EFFECT RAINBOW_BREATHE\n");
    sprintf(batchCmd2, "CMD DISP_EFFECT NONE\n");
    sendCommand2(input->fd, batchCmd1, batchCmd2);
}
