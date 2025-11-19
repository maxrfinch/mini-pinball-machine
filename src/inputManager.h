#ifndef HEADER_INPUT
#define HEADER_INPUT

typedef struct {
    int fd;
    int keyState;
    int leftKeyPressed;
    int rightKeyPressed;
    int centerKeyPressed;
} InputManager;

typedef enum {
    STATE_MENU,
    STATE_GAME,
    STATE_GAME_OVER
} InputGameState;

// Button LED modes (matching firmware)
typedef enum {
    LED_MODE_OFF = 0,
    LED_MODE_STEADY = 1,
    LED_MODE_BREATHE = 2,
    LED_MODE_BLINK = 3,
    LED_MODE_STROBE = 4
} InputLEDMode;

// Button indices (matching firmware)
#define BUTTON_LED_LEFT     0
#define BUTTON_LED_CENTER   1
#define BUTTON_LED_RIGHT    2

InputManager* inputInit();
void inputShutdown(InputManager* input);
void inputUpdate(InputManager* input);
int inputLeft(InputManager* input);
int inputRight(InputManager* input);
int inputCenter(InputManager* input);
int inputLeftPressed(InputManager* input);
int inputRightPressed(InputManager* input);
int inputCenterPressed(InputManager* input);
void inputSetGameState(InputManager* input, InputGameState state);
void inputSetScore(InputManager *input, long score);
void inputSetNumBalls(InputManager *input, int numBalls);

// Direct LED control (advanced/debug use only - prefer events)
void inputSetButtonLED(InputManager *input, int button_idx, InputLEDMode mode, int r, int g, int b, int count);

// High-level event API (preferred)
void inputSendEvent(InputManager *input, const char *event_name);
void inputSendGameStart(InputManager *input);
void inputSendBallReady(InputManager *input);
void inputSendBallLaunched(InputManager *input);

#endif
