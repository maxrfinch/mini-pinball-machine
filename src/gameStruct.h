#ifndef HEADER_GAME_STRUCT
#define HEADER_GAME_STRUCT
#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <box2d/box2d.h>
#include "inputManager.h"

typedef struct GameStructData GameStruct;

typedef struct SoundManagerObject {
    Music menuMusic;
    Music gameMusic;
    Sound* redPowerup;
    Sound* bluePowerup;
    Sound* slowdown;
    Sound* speedup;
    Sound* upperBouncer;
    Sound* click;
    Sound* bounce1;
    Sound* bounce2;
    Sound *flipper;
    Sound *waterSplash;
    Sound launch;
    Sound water;
    float gameMusicVolume;
} SoundManager;

typedef struct {
    int active;
    b2ShapeId shape;
    b2BodyId body;
    GameStruct *game;
    float locationHistoryX[16];
    float locationHistoryY[16];
    int trailStartIndex;
    int type;
    int killCounter;
    int underwaterState;
} Ball;

typedef enum {
    TRANSITION_TO_MENU,
    TRANSITION_TO_GAME,
    TRANSITION_GAME_OVER
} TransitionAction;

typedef enum {
    SCENE_RAYLIB_TITLE,   // Initial raylib loading screen
    SCENE_MENU,           // Main menu/attract mode
    SCENE_GAME,           // Active gameplay
    SCENE_GAME_OVER       // Score entry screen
} SceneId;

typedef enum {
    MODE_CLASSIC   // Currently only one mode exists
} GameMode;

typedef struct {
    b2ShapeId shape;
    b2BodyId body;
    float bounceEffect;
    int type;
    int enabled;
    float angle;
    float enabledSize;
} Bumper;


typedef struct {
    float px;
    float py;
    float vx;
    float vy;
} MenuPinball;

enum CollisionTypes {
    COLLISION_WALL = 0,
	COLLISION_BALL = 1,
    COLLISION_BUMPER = 2,
    COLLISION_PADDLE = 3,
    COLLISION_LEFT_LOWER_BUMPER = 4,
    COLLISION_RIGHT_LOWER_BUMPER = 5,
    COLLISION_ONE_WAY = 6
};

// Box2D collision category bits (bit flags for filtering)
// These map the Chipmunk collision types to Box2D's bit-based system
enum CollisionCategoryBits {
    CATEGORY_WALL = (1 << 0),              // 0x0001
    CATEGORY_BALL = (1 << 1),              // 0x0002
    CATEGORY_BUMPER = (1 << 2),            // 0x0004
    CATEGORY_PADDLE = (1 << 3),            // 0x0008
    CATEGORY_LEFT_LOWER_BUMPER = (1 << 4), // 0x0010
    CATEGORY_RIGHT_LOWER_BUMPER = (1 << 5),// 0x0020
    CATEGORY_ONE_WAY = (1 << 6)            // 0x0040
};

struct GameStructData {
    b2WorldId world;
    int numBalls;
    Ball *balls;
    int active;
    int gameState;  // Legacy: 0=menu, 1=game, 2=gameover, 5=title
    SceneId currentScene;
    SceneId nextScene;
    GameMode currentMode;
    GameMode pendingMode;
    long gameScore;
    long oldGameScore;
    int powerupScore;
    int powerupScoreDisplay;
    int transitionState;
    int transitionDelay;
    TransitionAction transitionTarget;
    float transitionAlpha;
    int numLives;
    int menuState;
    int nameSelectIndex;
    int nameSelectDone;
    int slowMotion;
    int slowMotionCounter;
    InputManager *input;
    float waterHeight;
    float waterHeightTarget;
    float waterHeightTimer;
    int waterPowerupState;
    int bumperPowerupState;
    int ballPowerupState;
    float redPowerupOverlay;
    float bluePowerupOverlay;
    float slowMotionFactor;
    SoundManager *sound;
    int leftFlipperState;
    int rightFlipperState;
    
    // Slow-mo powerup cooldown state
    int slowMoPowerupAvailable;      // 1 if powerup ready, 0 if in cooldown/explosion
    float slowMoCooldownTimer;       // Countdown timer for 20-second stay-alive requirement
    int slowMoCooldownBaselineLives; // Lives count when cooldown started (to detect ball loss)
    float slowMoExplosionEffect;     // Explosion animation intensity (0.0 to 1.0)

};

#endif
