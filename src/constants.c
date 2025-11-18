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
