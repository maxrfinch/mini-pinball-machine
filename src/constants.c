// Screen and world dimensions
const int screenWidth = 450;
const int screenHeight = 800;
const int worldWidth = 90;
const int worldHeight = 160;
const float worldToScreen = 5.0f;
const float screenToWorld = 0.2f;

// Flipper constants
const float flipperWidth = 19.0;
const float flipperHeight = 4.75;
const float flipperSpeed = 900.0f;
const float flipperRestAngleLeft = 33.0f;
const float flipperRestAngleRight = 147.0f;
const float flipperActiveAngleLeft = -43.0f;  // -33.0f - 10.0f
const float flipperActiveAngleRight = 223.0f; // 213.0f + 10.0f

// Ball constants
const int maxBalls = 256;
const float ballSize = 5.0f;

// Bumper constants
const int numBumpers = 14;
const float bumperSize = 10.0f;
const float smallBumperSize = 4.0f;
const float bumperBounciness = 1.8f;

// Powerup constants
const float powerupFullY = 64.0f;
const float powerupEmptyY = 104.4f;
const float powerupTargetScore = 5000.0f;
const float slowMoCooldownDuration = 20.0f;

// UI constants - Ball ready overlay
const int ballReadyOverlayX = 108;       // X position of ball ready overlay rectangle
const int ballReadyOverlayY = 600;       // Y position of ball ready overlay rectangle
const int ballReadyOverlayHeight = 80;   // Height of ball ready overlay rectangle
const int ballReadyPadding = 4;          // Padding for inner rectangle

const int ballReadyTextY = 610;          // Y position of "Ball X / Y" text
const float ballReadyTextSize = 40.0f;   // Font size for ball text

const int launchInstructionY = 650;      // Y position of launch instruction text
const float launchInstructionSize = 20.0f; // Font size for launch instruction

const int arrowAnimBaseY = 625;          // Base Y position for arrow animation
const int arrowAnimSpacing = 20;         // Spacing between arrows
const int arrowAnimCount = 8;            // Number of arrows to draw

// UI constants - Powerup overlay alpha multipliers
const float redPowerupOverlayAlpha = 40.0f;   // Alpha multiplier for red powerup overlay
const float bluePowerupOverlayAlpha = 128.0f; // Alpha multiplier for blue powerup overlay
const float icePowerupOverlayAlpha = 128.0f;  // Alpha multiplier for ice powerup overlay
