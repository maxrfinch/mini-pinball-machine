#ifndef HEADER_CONSTANTS
#define HEADER_CONSTANTS

// Screen and world dimensions
extern const int screenWidth;
extern const int screenHeight;
extern const int worldWidth;
extern const int worldHeight;
extern const float worldToScreen;
extern const float screenToWorld;

// Flipper constants
extern const float flipperWidth;
extern const float flipperHeight;
extern const float flipperSpeed;
extern const float flipperRestAngleLeft;
extern const float flipperRestAngleRight;
extern const float flipperActiveAngleLeft;
extern const float flipperActiveAngleRight;

// Ball constants
extern const int maxBalls;
extern const float ballSize;

// Bumper constants
extern const int numBumpers;
extern const float bumperSize;
extern const float smallBumperSize;
extern const float bumperBounciness;

// Water simulation constants
#define RIPPLE_SAMPLES 25
#define WATER_LEFT 0.0f
#define WATER_WIDTH 90.0f

// Powerup constants
extern const float powerupFullY;
extern const float powerupEmptyY;
extern const float powerupTargetScore;
extern const float slowMoCooldownDuration;

#endif
