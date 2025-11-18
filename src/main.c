#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <box2d/box2d.h>
#include "constants.h"
#include "physicsDebugDraw.h"
#include "inputManager.h"
#include "scores.h"
#include "soundManager.h"
#include "gameStruct.h"
#include "physics.h"
#include "resources.h"
#include "render.h"
#include "ui.h"

#define DEG_TO_RAD (3.14159265 / 180.0)
#define RAD_TO_DEG (180.0 / 3.14159265)

#if defined(PLATFORM_RPI)
    #define GLSL_VERSION            100
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            330
#endif

long long millis() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

const int maxBalls = 256;
const float ballSize = 5;

const int numBumpers = 14;
const float bumperSize = 10.0f;
const float smallBumperSize = 4.0f;
const float bumperBounciness = 1.8f;

// global pointer to the bumpers array
Bumper* bumpers = NULL;

static float slowMotionFactor = 1.0f;
static float iceOverlayAlpha = 0.0f;
static float waterImpactIntensity = 0.0f;

// leftLowerBumperAnim and rightLowerBumperAnim are now declared in physics.h
// and defined in physics.c (they are set by collision handlers)

static float multiballOverlayY = 0.0f;

// Water ripple simulation buffers
#define RIPPLE_SAMPLES 25
#define WATER_LEFT 0.0f
#define WATER_WIDTH 90.0f
static float rippleHeight[RIPPLE_SAMPLES];
static float rippleVelocity[RIPPLE_SAMPLES];

// AddWaterImpulse: Called from physics.c when ball hits water
void AddWaterImpulse(float x, float impulse) {
    int idx = (int)((x - WATER_LEFT) / WATER_WIDTH * RIPPLE_SAMPLES);
    if (idx >= 0 && idx < RIPPLE_SAMPLES) {
        rippleVelocity[idx] += impulse;
    }
}

// start game
void startGame(GameStruct *game){
    game->gameState = 1;
    game->numLives = 3;
    game->gameScore = 0;
    game->powerupScore = 0;
    game->powerupScoreDisplay = 0;
    game->bumperPowerupState = 0;
    game->ballPowerupState = 0;
    game->waterHeight = 0.0f;
    game->waterPowerupState = 0;
    game->redPowerupOverlay = 0;
    game->bluePowerupOverlay = 0;
    game->slowMotion=0;
    game->slowMotionCounter=0;
    game->leftFlipperState=0;
    game->rightFlipperState=0;
    iceOverlayAlpha=0.0f;
    
    // Initialize slow-mo powerup cooldown state (available at game start)
    game->slowMoPowerupAvailable = 1;
    game->slowMoCooldownTimer = 0.0f;
    game->slowMoCooldownBaselineLives = game->numLives;
    game->slowMoExplosionEffect = 0.0f;
    
    inputSetScore(game->input,0);
    inputSetGameState(game->input,STATE_GAME);
    inputSetNumBalls(game->input,game->numLives);
}

int main(void){

    // Initialize a struct encoding data about the game.
    GameStruct game;
    game.gameState = 0;


    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Mini Pinball by Chris Dalke!");
    SetTargetFPS(60);

    SoundManager *sound = initSound();
    game.sound = sound;

    // Initialize all resources (textures, shaders, fonts)
    Resources resources;
    Resources_Init(&resources);

    // Initialize ripple buffers
    for (int i = 0; i < RIPPLE_SAMPLES; i++) {
        rippleHeight[i] = 0.0f;
        rippleVelocity[i] = 0.0f;
    }
    
    // Shader parameters for amplitude scaling
    float ampX = 5.0f;
    float ampY = 5.0f;

    float shaderSeconds = 0.0f;

    // Initialize physics simulation
    b2BodyId* leftFlipperBody = NULL;
    b2BodyId* rightFlipperBody = NULL;
    physics_init(&game, &bumpers, &leftFlipperBody, &rightFlipperBody);
    float timeStep = 1.0/60.0;

    TraceLog(LOG_INFO, "PHYSICS INITIALIZED");

    // Flipper variables
    float flipperSpeed = 900.0f;
    float leftFlipperAngle = 33.0f;
    float rightFlipperAngle = 147.0f;
    float flipperSpeedScalar = 1.0f;

    // Powerup variables
    float powerupFullY = 64.0f;
    float powerupEmptyY = 104.4f;
    float powerupTargetScore = 5000.0f;
    TraceLog(LOG_INFO, "BALLS");

    //create balls array
    Ball* balls = malloc(maxBalls * sizeof(Ball));
    game.balls = balls;
    game.numBalls = 0;
    for (int i = 0; i < maxBalls; i++){
        balls[i].active = 0;
    }

    // Setup render texture for special ball effect
    RenderTexture2D renderTarget = LoadRenderTexture(screenWidth, screenHeight);

    // Menu setup
    MenuPinball* menuPinballs = malloc(32 * sizeof(MenuPinball));
    // Initialize pinballs
    for (int i = 0; i < 32; i++){
        menuPinballs[i].px = -100;
        menuPinballs[i].py = (rand() % screenHeight);
        menuPinballs[i].vx = 0;
        menuPinballs[i].vy = 0;
    }

    // Setup input
    InputManager *input = inputInit();
    game.input = input;

    // Setup score system
    ScoreHelper *scores = initScores();

    // Setup timestepping system
    int timestep = 1000.0/60.0;
    long long accumulatedTime = 0;
    long long startTime = millis();
    long long endTime = millis();
    long long elapsedTimeStart = millis();

    game.transitionState = 0;
    game.transitionAlpha = 0;
    game.transitionTarget = TRANSITION_TO_MENU;

    game.menuState = 0;

    //game.gameState = 2;
    game.nameSelectIndex = 0;
    game.nameSelectDone = 0;

    multiballOverlayY = 20 + worldHeight;

    game.gameState = 5;

    char tempString[128];
    char nameString[6];
    sprintf(nameString,"     ");

    inputSetGameState(input,STATE_MENU);
    TraceLog(LOG_INFO, "START");

    // Debug draw toggle state
    int debugDrawEnabled = 0;

    while (!WindowShouldClose()){
        endTime = millis();
        accumulatedTime += (endTime - startTime);
        startTime = millis();
        shaderSeconds += GetFrameTime() / 2.0f;
        float secondsVec[2] = { shaderSeconds, 0.0f };
        SetShaderValue(resources.swirlShader, resources.swirlSecondsLoc, secondsVec, SHADER_UNIFORM_VEC2);

        // Decay water impact intensity and drive ripple amplitude
        waterImpactIntensity *= 0.95f;
        if (waterImpactIntensity < 0.0f) waterImpactIntensity = 0.0f;
        if (waterImpactIntensity > 1.0f) waterImpactIntensity = 1.0f;

        float ampScale = 1.0f + 2.5f * waterImpactIntensity; // stronger ripples on impacts
        float ampXVecCurrent[2] = { ampX * ampScale, 0.0f };
        float ampYVecCurrent[2] = { ampY * ampScale, 0.0f };
        SetShaderValue(resources.swirlShader, resources.swirlAmpXLoc, ampXVecCurrent, SHADER_UNIFORM_VEC2);
        SetShaderValue(resources.swirlShader, resources.swirlAmpYLoc, ampYVecCurrent, SHADER_UNIFORM_VEC2);

        // Update ripple simulation
        float t = GetTime();
        
        // Add idle wave motion
        for (int i = 0; i < RIPPLE_SAMPLES; i++) {
            rippleHeight[i] += 0.002f * sinf(t * 1.5f + i * 0.15f);
        }

        // Neighbor propagation (spring-like behavior)
        float spread = 0.25f;
        for (int i = 1; i < RIPPLE_SAMPLES - 1; i++) {
            rippleVelocity[i] += spread * (rippleHeight[i-1] + rippleHeight[i+1] - 2.0f * rippleHeight[i]);
        }

        // Apply damping and update positions
        for (int i = 0; i < RIPPLE_SAMPLES; i++) {
            rippleVelocity[i] *= 0.985f;
            rippleHeight[i] += rippleVelocity[i];
        }

        // Update ripple texture
        unsigned char rippleData[RIPPLE_SAMPLES * 4];
        for (int i = 0; i < RIPPLE_SAMPLES; i++) {
            // Convert float height to grayscale byte (centered at 128)
            int val = (int)(128.0f + rippleHeight[i] * 50.0f);
            if (val < 0) val = 0;
            if (val > 255) val = 255;
            rippleData[i * 4 + 0] = (unsigned char)val;
            rippleData[i * 4 + 1] = (unsigned char)val;
            rippleData[i * 4 + 2] = (unsigned char)val;
            rippleData[i * 4 + 3] = 255;
        }
        UpdateTexture(resources.rippleTexture, rippleData);

        // Update water shader uniforms
        SetShaderValue(resources.waterShader, resources.waterSecondsLoc, secondsVec, SHADER_UNIFORM_VEC2);
        float ampXVecWater[2] = { ampX * ampScale, 0.0f };
        float ampYVecWater[2] = { ampY * ampScale, 0.0f };
        SetShaderValue(resources.waterShader, resources.waterAmpXLoc, ampXVecWater, SHADER_UNIFORM_VEC2);
        SetShaderValue(resources.waterShader, resources.waterAmpYLoc, ampYVecWater, SHADER_UNIFORM_VEC2);
        SetShaderValue(resources.waterShader, resources.waterLevelLoc, &game.waterHeight, SHADER_UNIFORM_FLOAT);
        SetShaderValueTexture(resources.waterShader, resources.waterRippleTexLoc, resources.rippleTexture);

        float mouseX = GetMouseX();
        float mouseY = GetMouseY();

        // Poll input
        inputUpdate(input);

        // Validate slowMotionFactor to prevent bad timesteps
        if (!isfinite(slowMotionFactor) || slowMotionFactor <= 0.0f) {
            TraceLog(LOG_WARNING, "slowMotionFactor invalid (%f), resetting", slowMotionFactor);
            slowMotionFactor = 1.0f;
        }

        game.slowMotionFactor = slowMotionFactor;

        // STEP SIMULATION AT FIXED RATE with safety cap
        const int MAX_PHYSICS_STEPS_PER_FRAME = 16;
        int stepCount = 0;
        while (accumulatedTime > timestep && stepCount < MAX_PHYSICS_STEPS_PER_FRAME){
            accumulatedTime -= timestep;
            stepCount++;

            updateSound(sound,&game);

            if (game.transitionState == 1){
                // TRANSITION OUT
                game.transitionAlpha += 15;
                if (game.transitionAlpha >= 255){
                    game.transitionState = 2;
                    game.transitionAlpha = 255;
                    game.transitionDelay = 0;
                }
            } else if (game.transitionState == 2){
                // HANDLE LOAD
                switch (game.transitionTarget){
                    case TRANSITION_TO_GAME: {
                        startGame(&game);
                        bumpers[4].enabled = 1;
                        bumpers[5].enabled = 1;
                        bumpers[6].enabled = 1;
                        bumpers[7].enabled = 1;
                        bumpers[8].enabled = 1;
                        bumpers[9].enabled = 1;
                        bumpers[10].enabled = 0;
                        bumpers[11].enabled = 0;
                        bumpers[12].enabled = 0;
                        bumpers[13].enabled = 0;
                        break;
                    }
                    case TRANSITION_TO_MENU: {
                        game.gameState = 0;
                        break;
                    }
                    case TRANSITION_GAME_OVER: {
                        game.gameState = 2;
                        game.nameSelectIndex = 0;
                        game.nameSelectDone = 0;
                        break;
                    }
                }
                game.transitionDelay++;
                if (game.transitionDelay > 10){
                    game.transitionState = 3;
                }
            } else if (game.transitionState == 3){
                //TRANSITION IN
                game.transitionAlpha -= 15;
                if (game.transitionAlpha <= 0){
                    game.transitionState = 0;
                    game.transitionAlpha = 0;
                }
            } else {
                game.transitionAlpha = 0;
            }
            if (game.gameState == 5){
                //transition from raylib title to menu
                if (game.transitionState == 0){
                    game.transitionState = 1;
                    game.transitionTarget = TRANSITION_TO_MENU;
                }
            }

            // Update menu pinballs
            for (int i = 0; i < 16; i++){
                menuPinballs[i].px += menuPinballs[i].vx;
                menuPinballs[i].py += menuPinballs[i].vy;
                menuPinballs[i].vy += 0.1f;
                if (menuPinballs[i].py > screenHeight + 20){
                    menuPinballs[i].px = 228;
                    menuPinballs[i].py = 126;
                    menuPinballs[i].vx = ((rand() % 40) / 10.0f) - 2.0f;
                    menuPinballs[i].vy = ((rand() % 50) / -10.0f);
                }
            }
            if (game.gameState == 0){
                if (inputCenterPressed(input)){
                    game.transitionState = 1;
                    game.transitionTarget = TRANSITION_TO_GAME;
                    playClick(sound);
                }
                if (inputLeftPressed(input))  {
                    playClick(sound);
                    game.menuState = 1;
                }
                if (inputRightPressed(input)) {
                    playClick(sound);
                    game.menuState = 0;
                }
            }
            float effectiveTimestep = (timeStep) * slowMotionFactor;
            if (game.gameState == 1){
                // Game

                // Clamp effective timestep to a sane, non-zero range to avoid numerical issues.
                if (effectiveTimestep < (1.0f / 600.0f)) {
                    effectiveTimestep = 1.0f / 600.0f;
                }
                if (effectiveTimestep > (1.0f / 20.0f)) {
                    effectiveTimestep = 1.0f / 20.0f;
                }

                physics_step(&game, effectiveTimestep);

                // TraceLog(LOG_INFO, "STEP END accumulatedTime=%lld", accumulatedTime);

                if (game.oldGameScore != game.gameScore){
                    inputSetScore(input,game.gameScore);
                    game.oldGameScore = game.gameScore;
                }
                // Check powerups before dispensing balls
                if (game.ballPowerupState == 0 && !bumpers[7].enabled && !bumpers[8].enabled && !bumpers[9].enabled){
                    // spawn balls
                    for (int i =0; i < 3; i++){
                        physics_add_ball(&game,89.5 - ballSize / 2,160 - (i * ballSize),0,-220,1);
                    }
                    playBluePowerupSound(sound);
                    game.bluePowerupOverlay = 1.0f;
                    game.ballPowerupState = -1;
                    game.gameScore += 500;
                    if (game.waterPowerupState == 0){
                        game.powerupScore += 500;
                    }
                } else if (game.ballPowerupState == -1){
                    // Check if there are no balls left. Then powerup resets and bumpers reset.
                    if (game.numBalls == 0){
                        game.ballPowerupState = 0;
                        bumpers[7].enabled = 1;
                        bumpers[8].enabled = 1;
                        bumpers[9].enabled = 1;
                    }
                }

                if (game.bumperPowerupState ==0 && !bumpers[4].enabled && !bumpers[5].enabled && !bumpers[6].enabled){
                    // spawn bumpers
                    game.bumperPowerupState = -1;
                    bumpers[10].enabled = 1;
                    bumpers[11].enabled = 1;
                    bumpers[12].enabled = 1;
                    bumpers[13].enabled = 1;
                    playRedPowerupSound(sound);
                    game.redPowerupOverlay = 1.0f;
                    game.gameScore += 500;
                    if (game.waterPowerupState == 0){
                        game.powerupScore += 500;
                    }
                } else if (game.bumperPowerupState == -1){
                    if (!bumpers[10].enabled && !bumpers[11].enabled && !bumpers[12].enabled && !bumpers[13].enabled){
                        game.bumperPowerupState = 0;
                        bumpers[4].enabled = 1;
                        bumpers[5].enabled = 1;
                        bumpers[6].enabled = 1;
                        game.redPowerupOverlay = 1.0f;
                    }
                }

                if (game.numBalls == 0){
                    if (game.numLives >= 1){
                        if (inputCenterPressed(input)){
                            physics_add_ball(&game,89.5 - ballSize / 2,160,0,-220,0);
                        }
                    } else {
                        // game over condition
                        if (game.transitionState == 0){
                            game.transitionState = 1;
                            game.transitionTarget = TRANSITION_GAME_OVER;
                            inputSetGameState(input,STATE_GAME_OVER);
                        }
                    }
                }

                if (IsMouseButtonPressed(0)){
                    physics_add_ball(&game,(mouseX) * screenToWorld,(mouseY) * screenToWorld,0,0,1);
                }

                float oldAngleLeft = leftFlipperAngle;
                float oldAngleRight = rightFlipperAngle;
                float targetAngleLeft = 0.0f;
                float targetAngleRight = 0.0f;
                if (inputLeft(input)){
                    if (game.leftFlipperState == 0){
                        playFlipper(sound);
                        game.leftFlipperState = 1;
                    }
                    targetAngleLeft = -33.0f - 10.0f;
                    leftFlipperAngle -= (flipperSpeed * effectiveTimestep);
                    if (leftFlipperAngle < targetAngleLeft){
                        leftFlipperAngle = targetAngleLeft;
                    }
                } else {
                    if (game.leftFlipperState == 1){
                        playFlipper(sound);
                    }
                    game.leftFlipperState = 0;
                    targetAngleLeft = 33.0f;
                    leftFlipperAngle += (flipperSpeed * effectiveTimestep);
                    if (leftFlipperAngle > targetAngleLeft){
                        leftFlipperAngle = targetAngleLeft;
                    }
                }
                if (inputRight(input)){
                    if (game.rightFlipperState == 0){
                        playFlipper(sound);
                        game.rightFlipperState = 1;
                    }
                    targetAngleRight = 213.0f + 10.0f;
                    rightFlipperAngle += (flipperSpeed * effectiveTimestep);
                    if (rightFlipperAngle > targetAngleRight){
                        rightFlipperAngle = targetAngleRight;
                    }
                } else {
                    if (game.rightFlipperState == 1){
                        playFlipper(sound);
                    }
                    game.rightFlipperState = 0;
                    targetAngleRight = 147.0f;
                    rightFlipperAngle -= (flipperSpeed * effectiveTimestep);
                    if (rightFlipperAngle < targetAngleRight){
                        rightFlipperAngle = targetAngleRight;
                    }
                }

                float deltaAngularVelocityLeft = 0.0f;
                float deltaAngularVelocityRight = 0.0f;
                if (effectiveTimestep > 0.0f) {
                    deltaAngularVelocityLeft = ((leftFlipperAngle * DEG_TO_RAD) - (oldAngleLeft * DEG_TO_RAD)) / effectiveTimestep;
                    deltaAngularVelocityRight = ((rightFlipperAngle * DEG_TO_RAD) - (oldAngleRight * DEG_TO_RAD)) / effectiveTimestep;
                }

                b2Rot leftRot = b2MakeRot(leftFlipperAngle * DEG_TO_RAD);
                b2Rot rightRot = b2MakeRot(rightFlipperAngle * DEG_TO_RAD);
                b2Body_SetTransform(*leftFlipperBody, b2Body_GetPosition(*leftFlipperBody), leftRot);
                b2Body_SetTransform(*rightFlipperBody, b2Body_GetPosition(*rightFlipperBody), rightRot);
                b2Body_SetAngularVelocity(*leftFlipperBody, deltaAngularVelocityLeft * flipperSpeedScalar);
                b2Body_SetAngularVelocity(*rightFlipperBody, deltaAngularVelocityRight * flipperSpeedScalar);

                // Check if any balls have fallen outside the screen
                // Remove them if they have.
                // Check if any balls are standing still for too long and remove.
                for (int i = 0; i < maxBalls; i++){
                    if (balls[i].active == 1){
                        b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                        b2Vec2 vel = b2Body_GetLinearVelocity(balls[i].body);
                        float velLengthSq = vel.x * vel.x + vel.y * vel.y;
                        if (velLengthSq < 0.01f){
                            balls[i].killCounter++;
                        } else {
                            balls[i].killCounter=0;
                        }
                        // Reset kill counter near flippers
                        if (pos.y > 118){
                            balls[i].killCounter=0;
                        }
                        if (pos.y > 170+ballSize || balls[i].killCounter > 100){
                            balls[i].active = 0;
                            b2DestroyBody(balls[i].body);
                            game.numBalls--;
                            //Check number of lives and send to score if necessary
                            if (game.numBalls == 0){
                                if (game.numLives >= 1){
                                    game.numLives -= 1;
                                    inputSetNumBalls(input,game.numLives);
                                }
                            }
                        }
                    }
                }

                //Update ball trails
                for (int i = 0; i < maxBalls; i++){
                    if (balls[i].active == 1){
                        b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                        balls[i].locationHistoryX[balls[i].trailStartIndex] = pos.x;
                        balls[i].locationHistoryY[balls[i].trailStartIndex] = pos.y;
                        balls[i].trailStartIndex = (balls[i].trailStartIndex + 1) % 16;
                    }
                }

                //handler lower bumpers
                if (leftLowerBumperAnim > 0.0f){
                    leftLowerBumperAnim -= 0.05f;
                    if (leftLowerBumperAnim < 0.0f){
                        leftLowerBumperAnim = 0.0f;
                    }
                }
                if (rightLowerBumperAnim > 0.0f){
                    rightLowerBumperAnim -= 0.05f;
                    if (rightLowerBumperAnim < 0.0f){
                        rightLowerBumperAnim = 0.0f;
                    }
                }

                //update ice overlay
                if (game.slowMotion == 1){
                    slowMotionFactor = 0.3f;
                    iceOverlayAlpha += 0.01f;
                    if (iceOverlayAlpha >= 1.0f){
                        iceOverlayAlpha = 1.0f;
                    }
                } else {
                    if (slowMotionFactor < 1.0f){
                        slowMotionFactor += 0.05f;
                        if (slowMotionFactor > 1.0f){
                            slowMotionFactor = 1.0f;
                        }
                    }
                    iceOverlayAlpha -= 0.01f;
                    if (iceOverlayAlpha <= 0.0f){
                        iceOverlayAlpha = 0.0f;
                    }
                }
                if (game.slowMotionCounter > 0){
                    game.slowMotionCounter--;
                    if (game.slowMotionCounter <= 0){
                        game.slowMotion = 0;
                        playSpeedupSound(sound);
                        
                        // Start 20-second cooldown timer after slow-mo ends
                        if (game.slowMoPowerupAvailable == 0) {
                            game.slowMoCooldownTimer = 20.0f; // 20 seconds
                            game.slowMoCooldownBaselineLives = game.numLives;
                        }
                    }
                }
                
                // Update explosion effect decay (affected by slow-motion for consistency)
                if (game.slowMoExplosionEffect > 0.0f) {
                    game.slowMoExplosionEffect -= 0.05f * slowMotionFactor;
                    if (game.slowMoExplosionEffect < 0.0f) {
                        game.slowMoExplosionEffect = 0.0f;
                    }
                }
                
                // Update slow-mo cooldown timer (20-second stay-alive requirement)
                if (game.slowMoCooldownTimer > 0.0f && game.slowMoPowerupAvailable == 0) {
                    // Count down using effectiveTimestep for consistency
                    game.slowMoCooldownTimer -= effectiveTimestep;
                    
                    // Check if ball was lost (lives decreased)
                    if (game.numLives < game.slowMoCooldownBaselineLives) {
                        // Ball lost - reset timer and update baseline
                        game.slowMoCooldownTimer = 20.0f;
                        game.slowMoCooldownBaselineLives = game.numLives;
                    }
                    
                    // Check if cooldown complete
                    if (game.slowMoCooldownTimer <= 0.0f) {
                        game.slowMoCooldownTimer = 0.0f;
                        game.slowMoPowerupAvailable = 1;
                        // TODO: play slow-mo ready sound
                    }
                }

                // Update red and blue powerup overlays
                if (game.redPowerupOverlay > 0.0f){
                    game.redPowerupOverlay -= 0.02f * slowMotionFactor;
                    if (game.redPowerupOverlay <= 0.0f){
                        game.redPowerupOverlay = 0.0f;
                    }
                }
                if (game.bluePowerupOverlay > 0.0f){
                    game.bluePowerupOverlay -= 0.04f * slowMotionFactor;
                    if (game.bluePowerupOverlay <= 0.0f){
                        game.bluePowerupOverlay = 0.0f;
                    }
                }

                // Update bumper
                for (int i = 0; i < numBumpers; i++){
                    bumpers[i].bounceEffect *= 0.94;
                    if (bumpers[i].enabled){
                        bumpers[i].enabledSize += 0.1f;
                        if (bumpers[i].enabledSize > 1.0f){
                            bumpers[i].enabledSize = 1.0f;
                        }
                    } else {
                        bumpers[i].enabledSize -= 0.1f;
                        if (bumpers[i].enabledSize < 0.0f){
                            bumpers[i].enabledSize = 0.0f;
                        }
                    }
                }

                // Update powerup score display
                if (game.powerupScoreDisplay < game.powerupScore){
                    game.powerupScoreDisplay += 10;
                    if (game.powerupScoreDisplay > game.powerupScore){
                        game.powerupScoreDisplay = game.powerupScore;
                    }
                } else if (game.powerupScoreDisplay > game.powerupScore){
                    game.powerupScoreDisplay -= 20;
                    if (game.powerupScoreDisplay < game.powerupScore){
                        game.powerupScoreDisplay = game.powerupScore;
                    }
                }
                if (game.powerupScoreDisplay < 0){
                    game.powerupScoreDisplay = 0;
                }
                // If the powerup is full, dispense powerup
                if (game.powerupScoreDisplay >= powerupTargetScore){
                    game.powerupScore = 0;
                    game.waterHeightTarget = 0.5f;
                    game.waterHeightTimer = 400.0f;
                    game.waterPowerupState = 1;
                    playWater(sound);

                    game.gameScore += 1000;
                    if (game.waterPowerupState == 0){
                        game.powerupScore += 1000;
                    }
                }


                if (game.waterPowerupState == 1){
                    game.waterHeight += 0.006f * slowMotionFactor;
                    if (game.waterHeight > game.waterHeightTarget){
                        game.waterHeight = game.waterHeightTarget;
                    }
                } else if (game.waterPowerupState == 2) {
                    game.waterHeight -= 0.0005f * slowMotionFactor;
                    if (game.waterHeight < 0.0f){
                        game.waterHeight = 0.0f;
                        game.waterPowerupState = 0;
                    }
                }

                if (game.waterHeightTimer > 0.0f){
                    game.waterHeightTimer -= 1.0f * slowMotionFactor;
                    if (game.waterHeightTimer <= 0.0f){
                        game.waterHeightTarget = 0.0f;
                        game.waterPowerupState = 2;
                        printf("water timer runout\n");
                    }
                }

                // If water height powerup active, apply buoyancy forces to balls.
                if (game.waterHeight > 0){
                    float waterY = worldHeight * (1.0f - game.waterHeight);

                    for (int i = 0; i < maxBalls; i++){
                        if (balls[i].active == 1){
                            b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                            b2Vec2 vel = b2Body_GetLinearVelocity(balls[i].body);
                            if (pos.y > waterY){
                                float distUnderwater = fabs(waterY - pos.y);
                                float bVely = -200.0f + -(distUnderwater * 40.0f);
                                b2Vec2 force = {0, bVely};
                                b2Body_ApplyForceToCenter(balls[i].body, force, true);
                                // Apply special forces for flipper
                                float flipperForce = -1000.0f;
                                if (pos.x <= worldWidth / 2.0f && fabsf(deltaAngularVelocityLeft) > 0){
                                    b2Vec2 flipForce = {0, flipperForce};
                                    b2Body_ApplyForceToCenter(balls[i].body, flipForce, true);
                                }
                                if (pos.x >= worldWidth / 2.0f && fabsf(deltaAngularVelocityRight) > 0){
                                    b2Vec2 flipForce = {0, flipperForce};
                                    b2Body_ApplyForceToCenter(balls[i].body, flipForce, true);
                                }
                                if (balls[i].underwaterState == 0){
                                    playWaterSplash(sound);
                                    balls[i].underwaterState = 1;

                                    // Kick the water ripple intensity on splash so the shader waves react
                                    waterImpactIntensity += 0.6f;
                                    if (waterImpactIntensity > 1.5f) {
                                        waterImpactIntensity = 1.5f;
                                    }
                                }
                            } else {
                                balls[i].underwaterState = 0;
                            }
                        }
                    }

                }
            }
            if (game.gameState == 2){
                // Game over
                if (game.nameSelectDone == 0){
                    if (inputRightPressed(input)){
                        playClick(sound);
                        game.nameSelectIndex++;
                        if (game.nameSelectIndex > 5){
                            game.nameSelectIndex = 0;
                        }
                    }
                    if (inputLeftPressed(input)){
                        playClick(sound);
                        game.nameSelectIndex--;
                        if (game.nameSelectIndex < 0){
                            game.nameSelectIndex = 5;
                        }
                    }
                    if (inputCenterPressed(input)){
                        playClick(sound);
                        if (game.nameSelectIndex == 5){
                            // Name selection done
                            // Submit score and start transition to menu.
                            game.nameSelectDone = 1;
                            game.transitionState = 1;
                            game.transitionTarget = TRANSITION_TO_MENU;
                            submitScore(scores,nameString,game.gameScore);
                            printf("Game Over. score: %ld\n",game.gameScore);
                            inputSetGameState(input,STATE_MENU);
                        } else {
                            if (game.nameSelectIndex > 0){
                                while (game.nameSelectIndex-1 >= 0 && nameString[game.nameSelectIndex-1] == 32){
                                    game.nameSelectIndex--;
                                }
                            }
                            if (nameString[game.nameSelectIndex] < 65 || nameString[game.nameSelectIndex] > 90){
                                nameString[game.nameSelectIndex] = 65;
                            } else {
                                nameString[game.nameSelectIndex] = (nameString[game.nameSelectIndex] + 1);
                                if (nameString[game.nameSelectIndex] > 90){
                                    nameString[game.nameSelectIndex] = 32;
                                } else if (nameString[game.nameSelectIndex] < 65){
                                    nameString[game.nameSelectIndex] = 90;
                                }
                            }
                        }
                    }
                }
            }
        }

        // Check if physics fell behind and clamp accumulated time
        if (stepCount == MAX_PHYSICS_STEPS_PER_FRAME && accumulatedTime > timestep) {
            TraceLog(LOG_WARNING,
                     "Physics fell behind: accumulatedTime=%lld, clamping",
                     accumulatedTime);
            accumulatedTime = 0;
        }

        // RENDER AT SPEED GOVERNED BY RAYLIB
        BeginDrawing();
        if (game.gameState == 0){
            // Menu
            UI_DrawMenu(&game, &resources, menuPinballs, 16, scores, elapsedTimeStart, shaderSeconds);
        }
        if (game.gameState == 1){
            // Game
            Render_Gameplay(&game, &resources, bumpers, numBumpers, 
                           *leftFlipperBody, *rightFlipperBody,
                           shaderSeconds, iceOverlayAlpha, 
                           debugDrawEnabled, elapsedTimeStart);
        }
        if (game.gameState == 2){
            // Game Over
            UI_DrawGameOver(&game, &resources, menuPinballs, 16, nameString, elapsedTimeStart, shaderSeconds);
        }
        if (game.gameState == 5){
            ClearBackground(WHITE);
        }

        // Draw transition overlay if active
        UI_DrawTransition(&game, shaderSeconds);

        EndDrawing();
    }

    shutdownScores(scores);
    inputShutdown(input);
    shutdownSound(sound);
    physics_shutdown(&game);
    
    // Unload all resources
    Resources_Unload(&resources);
    
    // Free allocated memory
    free(game.balls);
    free(menuPinballs);
    
    CloseWindow();

    return 0;
}