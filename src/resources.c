#include "resources.h"
#include "gameMode.h"
#include <stdio.h>

#if defined(PLATFORM_RPI)
    #define GLSL_VERSION            100
#else
    #define GLSL_VERSION            330
#endif

#define RIPPLE_SAMPLES 25

void Resources_Init(Resources *res) {
    // Load fonts (shared across all modes)
    res->font1 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf", 80, 0, 0);
    res->font2 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf", 120, 0, 0);

    // Load mode-specific textures for default mode (Classic)
    const GameModeConfig *defaultConfig = GetModeConfig(MODE_CLASSIC);
    res->bgTex = LoadTexture(defaultConfig->textures.backgroundTexture);
    res->ballTex = LoadTexture(defaultConfig->textures.ballTexture);
    res->beachBallTex = LoadTexture(defaultConfig->textures.beachBallTexture);
    res->trailTex = LoadTexture(defaultConfig->textures.trailTexture);
    res->bumperTex = LoadTexture(defaultConfig->textures.bumperTexture);
    res->bumperLightTex = LoadTexture(defaultConfig->textures.bumperLightTexture);
    res->iceBumperTex = LoadTexture(defaultConfig->textures.iceBumperTexture);
    res->shockwaveTex = LoadTexture(defaultConfig->textures.shockwaveTexture);
    res->debugTex = LoadTexture(defaultConfig->textures.debugTexture);
    res->leftFlipperTex = LoadTexture(defaultConfig->textures.flipperLeftTexture);
    res->rightFlipperTex = LoadTexture(defaultConfig->textures.flipperRightTexture);
    res->bgMenu = LoadTexture(defaultConfig->textures.menuBackgroundTexture);
    res->titleOverlay = LoadTexture(defaultConfig->textures.titleOverlayTexture);
    res->menuOverlaySystem = LoadTexture(defaultConfig->textures.menuOverlaySystemTexture);
    res->menuOverlayControls = LoadTexture(defaultConfig->textures.menuOverlayControlsTexture);
    res->menuOverlayHighscores = LoadTexture(defaultConfig->textures.menuOverlayHighscoresTexture);
    res->menuOverlayTopScores = LoadTexture(defaultConfig->textures.menuOverlayTopScoresTexture);
    res->gameOverOverlay1 = LoadTexture(defaultConfig->textures.gameOverOverlay1Texture);
    res->gameOverOverlay2 = LoadTexture(defaultConfig->textures.gameOverOverlay2Texture);
    res->gameOverOverlay1Top3 = LoadTexture(defaultConfig->textures.gameOverOverlay1Top3Texture);
    res->gameOverOverlay2Top3 = LoadTexture(defaultConfig->textures.gameOverOverlay2Top3Texture);
    res->arrowRight = LoadTexture(defaultConfig->textures.arrowRightTexture);
    res->menuControls = LoadTexture(defaultConfig->textures.menuControlsTexture);
    res->transitionTex = LoadTexture(defaultConfig->textures.transitionTexture);
    res->waterTex = LoadTexture(defaultConfig->textures.waterTexture);
    res->waterOverlayTex = LoadTexture(defaultConfig->textures.waterOverlayTexture);
    res->particleTex = LoadTexture(defaultConfig->textures.particleTexture);
    res->iceOverlay = LoadTexture(defaultConfig->textures.iceOverlayTexture);
    res->bumper3 = LoadTexture(defaultConfig->textures.bumper3Texture);
    res->lowerBumperShock = LoadTexture(defaultConfig->textures.lowerBumperShockTexture);
    res->redPowerupOverlay = LoadTexture(defaultConfig->textures.redPowerupOverlayTexture);

    // Load shaders (shared across all modes)
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

    // Log shader parameter values during initialization
    TraceLog(LOG_DEBUG, "[SHADER] Swirl shader initialized: freqX=%.2f, freqY=%.2f, ampX=%.2f, ampY=%.2f, speedX=%.2f, speedY=%.2f",
             freqX, freqY, ampX, ampY, speedX, speedY);
    TraceLog(LOG_DEBUG, "[SHADER] Swirl shader locations: seconds=%d, freqX=%d, freqY=%d, ampX=%d, ampY=%d, speedX=%d, speedY=%d",
             res->swirlSecondsLoc, res->swirlFreqXLoc, res->swirlFreqYLoc, 
             res->swirlAmpXLoc, res->swirlAmpYLoc, res->swirlSpeedXLoc, res->swirlSpeedYLoc);

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

    // Log water shader parameter values during initialization
    TraceLog(LOG_DEBUG, "[SHADER] Water shader initialized: freqX=%.2f, freqY=%.2f, ampX=%.2f, ampY=%.2f, speedX=%.2f, speedY=%.2f",
             freqX, freqY, ampX, ampY, speedX, speedY);
    TraceLog(LOG_DEBUG, "[SHADER] Water shader locations: seconds=%d, freqX=%d, freqY=%d, ampX=%d, ampY=%d, speedX=%d, speedY=%d, rippleTex=%d, waterLevel=%d",
             res->waterSecondsLoc, res->waterFreqXLoc, res->waterFreqYLoc, 
             res->waterAmpXLoc, res->waterAmpYLoc, res->waterSpeedXLoc, res->waterSpeedYLoc,
             res->waterRippleTexLoc, res->waterLevelLoc);

    // Create ripple texture (25x1 R32F)
    Image rippleImage = GenImageColor(RIPPLE_SAMPLES, 1, (Color){0, 0, 0, 255});
    res->rippleTexture = LoadTextureFromImage(rippleImage);
    UnloadImage(rippleImage);
    SetTextureFilter(res->rippleTexture, TEXTURE_FILTER_BILINEAR);
    SetTextureWrap(res->rippleTexture, TEXTURE_WRAP_CLAMP);
}

void Resources_LoadModeTextures(Resources *res, const void *modeConfigPtr) {
    const GameModeConfig *config = (const GameModeConfig *)modeConfigPtr;
    
    // Unload existing mode-specific textures
    UnloadTexture(res->bgTex);
    UnloadTexture(res->ballTex);
    UnloadTexture(res->beachBallTex);
    UnloadTexture(res->trailTex);
    UnloadTexture(res->leftFlipperTex);
    UnloadTexture(res->rightFlipperTex);
    UnloadTexture(res->bumperTex);
    UnloadTexture(res->bumperLightTex);
    UnloadTexture(res->iceBumperTex);
    UnloadTexture(res->bumper3);
    UnloadTexture(res->shockwaveTex);
    UnloadTexture(res->lowerBumperShock);
    UnloadTexture(res->bgMenu);
    UnloadTexture(res->titleOverlay);
    UnloadTexture(res->menuOverlaySystem);
    UnloadTexture(res->menuOverlayControls);
    UnloadTexture(res->menuOverlayHighscores);
    UnloadTexture(res->menuOverlayTopScores);
    UnloadTexture(res->gameOverOverlay1);
    UnloadTexture(res->gameOverOverlay2);
    UnloadTexture(res->gameOverOverlay1Top3);
    UnloadTexture(res->gameOverOverlay2Top3);
    UnloadTexture(res->waterTex);
    UnloadTexture(res->waterOverlayTex);
    UnloadTexture(res->iceOverlay);
    UnloadTexture(res->redPowerupOverlay);
    UnloadTexture(res->particleTex);
    UnloadTexture(res->arrowRight);
    UnloadTexture(res->menuControls);
    UnloadTexture(res->transitionTex);
    UnloadTexture(res->debugTex);
    
    // Load mode-specific textures
    res->bgTex = LoadTexture(config->textures.backgroundTexture);
    res->ballTex = LoadTexture(config->textures.ballTexture);
    res->beachBallTex = LoadTexture(config->textures.beachBallTexture);
    res->trailTex = LoadTexture(config->textures.trailTexture);
    res->leftFlipperTex = LoadTexture(config->textures.flipperLeftTexture);
    res->rightFlipperTex = LoadTexture(config->textures.flipperRightTexture);
    res->bumperTex = LoadTexture(config->textures.bumperTexture);
    res->bumperLightTex = LoadTexture(config->textures.bumperLightTexture);
    res->iceBumperTex = LoadTexture(config->textures.iceBumperTexture);
    res->bumper3 = LoadTexture(config->textures.bumper3Texture);
    res->shockwaveTex = LoadTexture(config->textures.shockwaveTexture);
    res->lowerBumperShock = LoadTexture(config->textures.lowerBumperShockTexture);
    res->bgMenu = LoadTexture(config->textures.menuBackgroundTexture);
    res->titleOverlay = LoadTexture(config->textures.titleOverlayTexture);
    res->menuOverlaySystem = LoadTexture(config->textures.menuOverlaySystemTexture);
    res->menuOverlayControls = LoadTexture(config->textures.menuOverlayControlsTexture);
    res->menuOverlayHighscores = LoadTexture(config->textures.menuOverlayHighscoresTexture);
    res->menuOverlayTopScores = LoadTexture(config->textures.menuOverlayTopScoresTexture);
    res->gameOverOverlay1 = LoadTexture(config->textures.gameOverOverlay1Texture);
    res->gameOverOverlay2 = LoadTexture(config->textures.gameOverOverlay2Texture);
    res->gameOverOverlay1Top3 = LoadTexture(config->textures.gameOverOverlay1Top3Texture);
    res->gameOverOverlay2Top3 = LoadTexture(config->textures.gameOverOverlay2Top3Texture);
    res->waterTex = LoadTexture(config->textures.waterTexture);
    res->waterOverlayTex = LoadTexture(config->textures.waterOverlayTexture);
    res->iceOverlay = LoadTexture(config->textures.iceOverlayTexture);
    res->redPowerupOverlay = LoadTexture(config->textures.redPowerupOverlayTexture);
    res->particleTex = LoadTexture(config->textures.particleTexture);
    res->arrowRight = LoadTexture(config->textures.arrowRightTexture);
    res->menuControls = LoadTexture(config->textures.menuControlsTexture);
    res->transitionTex = LoadTexture(config->textures.transitionTexture);
    res->debugTex = LoadTexture(config->textures.debugTexture);
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
    UnloadTexture(res->menuOverlaySystem);
    UnloadTexture(res->menuOverlayControls);
    UnloadTexture(res->menuOverlayHighscores);
    UnloadTexture(res->menuOverlayTopScores);
    UnloadTexture(res->gameOverOverlay1);
    UnloadTexture(res->gameOverOverlay2);
    UnloadTexture(res->gameOverOverlay1Top3);
    UnloadTexture(res->gameOverOverlay2Top3);
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
