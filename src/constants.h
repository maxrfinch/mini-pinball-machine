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

// UI constants - Ball ready overlay
extern const int ballReadyOverlayX;       // X position of ball ready overlay rectangle
extern const int ballReadyOverlayY;       // Y position of ball ready overlay rectangle
extern const int ballReadyOverlayHeight;  // Height of ball ready overlay rectangle
extern const int ballReadyPadding;        // Padding for inner rectangle

extern const int ballReadyTextY;          // Y position of "Ball X / Y" text
extern const float ballReadyTextSize;     // Font size for ball text

extern const int launchInstructionY;      // Y position of launch instruction text
extern const float launchInstructionSize; // Font size for launch instruction

extern const int ballReadyOverlayMargin;  // Width margin for ball ready overlay (screenWidth - margin = width)
extern const int ballReadyInnerMargin;    // Width margin for inner rectangle

extern const int arrowAnimBaseY;          // Base Y position for arrow animation
extern const int arrowAnimSpacing;        // Spacing between arrows
extern const int arrowAnimCount;          // Number of arrows to draw

// UI constants - Powerup overlay alpha multipliers
extern const float redPowerupOverlayAlpha;   // Alpha multiplier for red powerup overlay
extern const float bluePowerupOverlayAlpha;  // Alpha multiplier for blue powerup overlay
extern const float icePowerupOverlayAlpha;   // Alpha multiplier for ice powerup overlay

// Trail history size for ball rendering
#define TRAIL_HISTORY_SIZE 16

#endif
