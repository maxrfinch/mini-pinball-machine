#ifndef HEADER_INPUT
#define HEADER_INPUT

typedef struct {
    int fd;
    int keyState;
    int leftKeyPressed;
    int rightKeyPressed;
    int centerKeyPressed;
    int centerKeyHeld;      // 1 if HELD event received since last DOWN
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
void inputShutdownEffects(InputManager* input);
void inputUpdate(InputManager* input);
int inputLeft(InputManager* input);
int inputRight(InputManager* input);
int inputCenter(InputManager* input);
int inputLeftPressed(InputManager* input);
int inputRightPressed(InputManager* input);
int inputCenterPressed(InputManager* input);
int inputCenterHeld(InputManager* input);   // Returns 1 if center HELD event received
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

// Display animation functions
void inputSendBallSavedAnimation(InputManager *input);
void inputSendMultiballAnimation(InputManager *input);

// Camera NeoPixel commands
void inputSendCameraPreview(InputManager *input);
void inputSendCameraFlash(InputManager *input);
void inputSendCameraIdle(InputManager *input);

// Water powerup NeoPixel/matrix effect
void inputSendWaterEffectStart(InputManager *input);
void inputSendWaterEffectEnd(InputManager *input);

// Matrix display effect functions
void inputSendIcedUpEffect(InputManager *input);
void inputSendHighScoreEffect(InputManager *input);
void inputSendGameOverCurtainEffect(InputManager *input);
void inputSendMenuEffect(InputManager *input);

// Charge mechanic functions
void inputSendChargeStatus(InputManager *input, int charge_percent);
void inputSendHapticStrength(InputManager *input, int strength_percent);

#endif
