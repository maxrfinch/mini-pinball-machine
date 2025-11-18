#ifndef RESOURCES_H
#define RESOURCES_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Resources {
    // Textures
    Texture2D bgTex;
    Texture2D ballTex;
    Texture2D beachBallTex;
    Texture2D trailTex;
    Texture2D bumperTex;
    Texture2D bumperLightTex;
    Texture2D iceBumperTex;
    Texture2D shockwaveTex;
    Texture2D debugTex;
    Texture2D leftFlipperTex;
    Texture2D rightFlipperTex;
    Texture2D bgMenu;
    Texture2D titleOverlay;
    Texture2D menuOverlay1;
    Texture2D gameOverOverlay1;
    Texture2D gameOverOverlay2;
    Texture2D arrowRight;
    Texture2D menuControls;
    Texture2D transitionTex;
    Texture2D waterTex;
    Texture2D waterOverlayTex;
    Texture2D particleTex;
    Texture2D iceOverlay;
    Texture2D bumper3;
    Texture2D lowerBumperShock;
    Texture2D redPowerupOverlay;
    Texture2D rippleTexture;

    // Fonts
    Font font1;
    Font font2;

    // Shaders
    Shader alphaTestShader;
    Shader swirlShader;
    Shader waterShader;
    
    // Shader locations for swirl shader
    int swirlSecondsLoc;
    int swirlFreqXLoc;
    int swirlFreqYLoc;
    int swirlAmpXLoc;
    int swirlAmpYLoc;
    int swirlSpeedXLoc;
    int swirlSpeedYLoc;
    
    // Shader locations for water shader
    int waterSecondsLoc;
    int waterFreqXLoc;
    int waterFreqYLoc;
    int waterAmpXLoc;
    int waterAmpYLoc;
    int waterSpeedXLoc;
    int waterSpeedYLoc;
    int waterRippleTexLoc;
    int waterLevelLoc;
} Resources;

// Initialize all resources (textures, shaders, fonts)
void Resources_Init(Resources *res);

// Unload all resources
void Resources_Unload(Resources *res);

#ifdef __cplusplus
}
#endif

#endif // RESOURCES_H
