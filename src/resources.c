#include "resources.h"
#include <stdio.h>

#if defined(PLATFORM_RPI)
    #define GLSL_VERSION            100
#else
    #define GLSL_VERSION            330
#endif

#define RIPPLE_SAMPLES 25

void Resources_Init(Resources *res) {
    // Load textures
    res->bgTex = LoadTexture("Resources/Textures/background2.png");
    res->ballTex = LoadTexture("Resources/Textures/ball.png");
    res->beachBallTex = LoadTexture("Resources/Textures/beachBall.png");
    res->trailTex = LoadTexture("Resources/Textures/trail.png");
    res->bumperTex = LoadTexture("Resources/Textures/bumper.png");
    res->bumperLightTex = LoadTexture("Resources/Textures/bumperLight.png");
    res->iceBumperTex = LoadTexture("Resources/Textures/iceBumper.png");
    res->shockwaveTex = LoadTexture("Resources/Textures/shockwave.png");
    res->debugTex = LoadTexture("Resources/Textures/debugSmall.png");
    res->leftFlipperTex = LoadTexture("Resources/Textures/flipperL.png");
    res->rightFlipperTex = LoadTexture("Resources/Textures/flipperR.png");
    res->bgMenu = LoadTexture("Resources/Textures/bgMenu.png");
    res->titleOverlay = LoadTexture("Resources/Textures/titleOverlay.png");
    res->menuOverlay1 = LoadTexture("Resources/Textures/menuOverlay1.png");
    res->gameOverOverlay1 = LoadTexture("Resources/Textures/gameOverOverlay1.png");
    res->gameOverOverlay2 = LoadTexture("Resources/Textures/gameOverOverlay2.png");
    res->arrowRight = LoadTexture("Resources/Textures/arrowRight.png");
    res->menuControls = LoadTexture("Resources/Textures/menuControls.png");
    res->transitionTex = LoadTexture("Resources/Textures/transition.png");
    res->waterTex = LoadTexture("Resources/Textures/waterTex.png");
    res->waterOverlayTex = LoadTexture("Resources/Textures/waterOverlayTex.png");
    res->particleTex = LoadTexture("Resources/Textures/particle.png");
    res->iceOverlay = LoadTexture("Resources/Textures/iceOverlay.png");
    res->bumper3 = LoadTexture("Resources/Textures/bumper3.png");
    res->lowerBumperShock = LoadTexture("Resources/Textures/lowerBumperShock.png");
    res->redPowerupOverlay = LoadTexture("Resources/Textures/redPowerupOverlay.png");

    // Load fonts
    res->font1 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf", 80, 0, 0);
    res->font2 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf", 120, 0, 0);

    // Load shaders
    res->alphaTestShader = LoadShader(0, TextFormat("Resources/Shaders/glsl%i/alphaTest.fs", GLSL_VERSION));

    res->swirlShader = LoadShader(0, TextFormat("Resources/Shaders/glsl%i/wave.fs", GLSL_VERSION));
    res->swirlSecondsLoc = GetShaderLocation(res->swirlShader, "secondes");
    res->swirlFreqXLoc = GetShaderLocation(res->swirlShader, "freqX");
    res->swirlFreqYLoc = GetShaderLocation(res->swirlShader, "freqY");
    res->swirlAmpXLoc = GetShaderLocation(res->swirlShader, "ampX");
    res->swirlAmpYLoc = GetShaderLocation(res->swirlShader, "ampY");
    res->swirlSpeedXLoc = GetShaderLocation(res->swirlShader, "speedX");
    res->swirlSpeedYLoc = GetShaderLocation(res->swirlShader, "speedY");

    float freqX = 25.0f;
    float freqY = 25.0f;
    float ampX = 5.0f;
    float ampY = 5.0f;
    float speedX = 8.0f;
    float speedY = 8.0f;

    // Get screen dimensions from constants (externally defined)
    extern const int screenWidth;
    extern const int screenHeight;
    float screenSize[2] = {(float)screenWidth, (float)screenHeight};

    // Pack scalars into vec2s (second component unused)
    float freqXVec[2]   = { freqX,   0.0f };
    float freqYVec[2]   = { freqY,   0.0f };
    float ampXVec[2]    = { ampX,    0.0f };
    float ampYVec[2]    = { ampY,    0.0f };
    float speedXVec[2]  = { speedX,  0.0f };
    float speedYVec[2]  = { speedY,  0.0f };

    SetShaderValue(res->swirlShader, GetShaderLocation(res->swirlShader, "size"), &screenSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlFreqXLoc,  freqXVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlFreqYLoc,  freqYVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlAmpXLoc,   ampXVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlAmpYLoc,   ampYVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlSpeedXLoc, speedXVec, SHADER_UNIFORM_VEC2);
    SetShaderValue(res->swirlShader, res->swirlSpeedYLoc, speedYVec, SHADER_UNIFORM_VEC2);

    // Load water shader with ripple support
    res->waterShader = LoadShader(0, TextFormat("Resources/Shaders/glsl%i/water.fs", GLSL_VERSION));
    res->waterSecondsLoc = GetShaderLocation(res->waterShader, "secondes");
    res->waterFreqXLoc = GetShaderLocation(res->waterShader, "freqX");
    res->waterFreqYLoc = GetShaderLocation(res->waterShader, "freqY");
    res->waterAmpXLoc = GetShaderLocation(res->waterShader, "ampX");
    res->waterAmpYLoc = GetShaderLocation(res->waterShader, "ampY");
    res->waterSpeedXLoc = GetShaderLocation(res->waterShader, "speedX");
    res->waterSpeedYLoc = GetShaderLocation(res->waterShader, "speedY");
    res->waterRippleTexLoc = GetShaderLocation(res->waterShader, "rippleTex");
    res->waterLevelLoc = GetShaderLocation(res->waterShader, "waterLevel");

    SetShaderValue(res->waterShader, GetShaderLocation(res->waterShader, "size"), &screenSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterFreqXLoc,  freqXVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterFreqYLoc,  freqYVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterAmpXLoc,   ampXVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterAmpYLoc,   ampYVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterSpeedXLoc, speedXVec, SHADER_UNIFORM_VEC2);
    SetShaderValue(res->waterShader, res->waterSpeedYLoc, speedYVec, SHADER_UNIFORM_VEC2);

    // Create ripple texture (25x1 R32F)
    Image rippleImage = GenImageColor(RIPPLE_SAMPLES, 1, (Color){0, 0, 0, 255});
    res->rippleTexture = LoadTextureFromImage(rippleImage);
    UnloadImage(rippleImage);
    SetTextureFilter(res->rippleTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(res->rippleTexture, TEXTURE_WRAP_CLAMP);
}

void Resources_Unload(Resources *res) {
    // Unload textures
    UnloadTexture(res->bgTex);
    UnloadTexture(res->ballTex);
    UnloadTexture(res->beachBallTex);
    UnloadTexture(res->trailTex);
    UnloadTexture(res->bumperTex);
    UnloadTexture(res->bumperLightTex);
    UnloadTexture(res->iceBumperTex);
    UnloadTexture(res->shockwaveTex);
    UnloadTexture(res->debugTex);
    UnloadTexture(res->leftFlipperTex);
    UnloadTexture(res->rightFlipperTex);
    UnloadTexture(res->bgMenu);
    UnloadTexture(res->titleOverlay);
    UnloadTexture(res->menuOverlay1);
    UnloadTexture(res->gameOverOverlay1);
    UnloadTexture(res->gameOverOverlay2);
    UnloadTexture(res->arrowRight);
    UnloadTexture(res->menuControls);
    UnloadTexture(res->transitionTex);
    UnloadTexture(res->waterTex);
    UnloadTexture(res->waterOverlayTex);
    UnloadTexture(res->particleTex);
    UnloadTexture(res->iceOverlay);
    UnloadTexture(res->bumper3);
    UnloadTexture(res->lowerBumperShock);
    UnloadTexture(res->redPowerupOverlay);
    UnloadTexture(res->rippleTexture);

    // Unload fonts
    UnloadFont(res->font1);
    UnloadFont(res->font2);

    // Unload shaders
    UnloadShader(res->alphaTestShader);
    UnloadShader(res->swirlShader);
    UnloadShader(res->waterShader);
}
