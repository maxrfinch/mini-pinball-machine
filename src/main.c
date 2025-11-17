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

static float leftLowerBumperAnim = 0.0f;
static float rightLowerBumperAnim = 0.0f;

static float multiballOverlayY = 0.0f;





// Debug drawing functions (commented out - need Box2D implementation)
// TODO: Implement Box2D debug draw callbacks
/*
static void
ChipmunkDebugDrawCirclePointer(cpVect p, cpFloat a, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{ChipmunkDebugDrawCircle(p, a, r, outline, fill);}

static void
ChipmunkDebugDrawSegmentPointer(cpVect a, cpVect b, cpSpaceDebugColor color, cpDataPointer data)
{ChipmunkDebugDrawSegment(a, b, color);}

static void
ChipmunkDebugDrawFatSegmentPointer(cpVect a, cpVect b, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{ChipmunkDebugDrawFatSegment(a, b, r, outline, fill);}

static void
ChipmunkDebugDrawPolygonPointer(int count, const cpVect *verts, cpFloat r, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{ChipmunkDebugDrawPolygon(count, verts, r, outline, fill);}

static void
ChipmunkDebugDrawDotPointer(cpFloat size, cpVect pos, cpSpaceDebugColor color, cpDataPointer data)
{ChipmunkDebugDrawDot(size, pos, color);}
*/


// start game
void startGame(GameStruct *game){
    game->gameState = 1;
    game->numLives = 5;
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


    Texture bgTex = LoadTexture("Resources/Textures/background2.png");
    Texture ballTex = LoadTexture("Resources/Textures/ball.png");
    Texture beachBallTex = LoadTexture("Resources/Textures/beachBall.png");
    Texture trailTex = LoadTexture("Resources/Textures/trail.png");
    Texture bumperTex = LoadTexture("Resources/Textures/bumper.png");
    Texture bumperLightTex = LoadTexture("Resources/Textures/bumperLight.png");
    Texture iceBumperTex = LoadTexture("Resources/Textures/iceBumper.png");
    Texture shockwaveTex = LoadTexture("Resources/Textures/shockwave.png");
    Texture debugTex = LoadTexture("Resources/Textures/debugSmall.png");
    Texture leftFlipperTex = LoadTexture("Resources/Textures/flipperL.png");
    Texture rightFlipperTex = LoadTexture("Resources/Textures/flipperR.png");
    Texture bgMenu = LoadTexture("Resources/Textures/bgMenu.png");
    Texture titleOverlay = LoadTexture("Resources/Textures/titleOverlay.png");
    Texture menuOverlay1 = LoadTexture("Resources/Textures/menuOverlay1.png");
    Texture gameOverOverlay1 = LoadTexture("Resources/Textures/gameOverOverlay1.png");
    Texture gameOverOverlay2 = LoadTexture("Resources/Textures/gameOverOverlay2.png");
    Texture arrowRight = LoadTexture("Resources/Textures/arrowRight.png");
    Texture menuControls = LoadTexture("Resources/Textures/menuControls.png");
    Texture transitionTex = LoadTexture("Resources/Textures/transition.png");
    Texture waterTex = LoadTexture("Resources/Textures/waterTex.png");
    Texture waterOverlayTex = LoadTexture("Resources/Textures/waterOverlayTex.png");
    Texture particleTex = LoadTexture("Resources/Textures/particle.png");
    Texture iceOverlay = LoadTexture("Resources/Textures/iceOverlay.png");
    Texture bumper3 = LoadTexture("Resources/Textures/bumper3.png");
    Texture lowerBumperShock = LoadTexture("Resources/Textures/lowerBumperShock.png");
    Texture redPowerupOverlay = LoadTexture("Resources/Textures/redPowerupOverlay.png");

    Font font1 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf",80,0,0);
    Font font2 = LoadFontEx("Resources/Fonts/Avenir-Black.ttf",120,0,0);

    Shader alphaTestShader = LoadShader(0, TextFormat("resources/shaders/glsl%i/alphaTest.fs", GLSL_VERSION));

    Shader swirlShader = LoadShader(0, TextFormat("Resources/Shaders/glsl%i/wave.fs", GLSL_VERSION));
    int secondsLoc = GetShaderLocation(swirlShader, "secondes");
	int freqXLoc = GetShaderLocation(swirlShader, "freqX");
	int freqYLoc = GetShaderLocation(swirlShader, "freqY");
	int ampXLoc = GetShaderLocation(swirlShader, "ampX");
	int ampYLoc = GetShaderLocation(swirlShader, "ampY");
	int speedXLoc = GetShaderLocation(swirlShader, "speedX");
	int speedYLoc = GetShaderLocation(swirlShader, "speedY");
	
    float freqX = 25.0f;
    float freqY = 25.0f;
    float ampX = 5.0f;
    float ampY = 5.0f;
    float speedX = 8.0f;
    float speedY = 8.0f;
    float screenSize[2] = {screenWidth, screenHeight};

    // Pack scalars into vec2s (second component unused)
    float freqXVec[2]   = { freqX,   0.0f };
    float freqYVec[2]   = { freqY,   0.0f };
    float ampXVec[2]    = { ampX,    0.0f };
    float ampYVec[2]    = { ampY,    0.0f };
    float speedXVec[2]  = { speedX,  0.0f };
    float speedYVec[2]  = { speedY,  0.0f };

    SetShaderValue(swirlShader, GetShaderLocation(swirlShader, "size"), &screenSize, SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, freqXLoc,  freqXVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, freqYLoc,  freqYVec,  SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, ampXLoc,   ampXVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, ampYLoc,   ampYVec,   SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, speedXLoc, speedXVec, SHADER_UNIFORM_VEC2);
    SetShaderValue(swirlShader, speedYLoc, speedYVec, SHADER_UNIFORM_VEC2);

    float shaderSeconds = 0.0f;

    TraceLog(LOG_INFO, "HELLO");

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


    // Setup debug draw options (commented out for Box2D migration)
    // TODO: Implement Box2D debug draw
    /*
    cpSpaceDebugDrawOptions drawOptions = {
    	ChipmunkDebugDrawCirclePointer,
    	ChipmunkDebugDrawSegmentPointer,
    	ChipmunkDebugDrawFatSegmentPointer,
    	ChipmunkDebugDrawPolygonPointer,
    	ChipmunkDebugDrawDotPointer,
    	(cpSpaceDebugDrawFlags)(CP_SPACE_DEBUG_DRAW_SHAPES | CP_SPACE_DEBUG_DRAW_CONSTRAINTS | CP_SPACE_DEBUG_DRAW_COLLISION_POINTS),
    	{0xEE/255.0f, 0xE8/255.0f, 0xD5/255.0f, 1.0f}, // Outline color
    	ChipmunkDebugGetColorForShape,
    	{0.0f, 0.75f, 0.0f, 1.0f}, // Constraint color
    	{1.0f, 0.0f, 0.0f, 1.0f}, // Collision point color
    	NULL,
    };
    */

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

    while (!WindowShouldClose()){
        endTime = millis();
        accumulatedTime += (endTime - startTime);
        startTime = millis();
        shaderSeconds += GetFrameTime() / 2.0f;
        float secondsVec[2] = { shaderSeconds, 0.0f };
        SetShaderValue(swirlShader, secondsLoc, secondsVec, SHADER_UNIFORM_VEC2);

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

                // Optional: uncomment for verbose physics-step logging while debugging hangs
                 TraceLog(LOG_INFO, "STEP START accumulatedTime=%lld, effectiveTimestep=%f, slowMotionFactor=%f", 
                         accumulatedTime, effectiveTimestep, slowMotionFactor);

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
            ClearBackground((Color){255,183,0,255});
            float timeFactor = (millis() - elapsedTimeStart) / 1000.0f;
            float xOffset = sin(timeFactor) * 50.0f;
            float yOffset = cos(timeFactor) * 50.0f;
            float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
            float width = screenWidth * 3;
            float height = screenHeight * 3;
			BeginShaderMode(swirlShader);
            DrawTexturePro(bgMenu,(Rectangle){0,0,bgMenu.width,bgMenu.height},(Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},(Vector2){width/2,height/2},angle,WHITE);
            EndShaderMode();

            // Render pinballs
            for (int i = 0; i < 16; i++){
                DrawTexturePro(ballTex,(Rectangle){0,0,ballTex.width,ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){255,183,0,255});
            }

            DrawTexturePro(menuOverlay1,(Rectangle){0,0,titleOverlay.width,titleOverlay.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);
            DrawTexturePro(titleOverlay,(Rectangle){0,0,titleOverlay.width,titleOverlay.height},(Rectangle){0,12 + sin(timeFactor)*5.0f,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

            if (game.menuState == 0){
                DrawTextEx(font1, "Top Scores", (Vector2){153,329}, 36.0, 1.0, WHITE);
                float y = 362;
                for (int i = 1; i <= 10; i++){
                    ScoreObject *score = getRankedScore(scores,i);
                    if (score != NULL){
                        sprintf(tempString,"%d)",i);
                        DrawTextEx(font1, tempString, (Vector2){66 - MeasureTextEx(font1, tempString, 27.0, 1.0).x,y}, 27.0, 1.0, WHITE);
                        sprintf(tempString,"%s",score->scoreName);
                        DrawTextEx(font1, tempString, (Vector2){75,y}, 27.0, 1.0, WHITE);
                        float scoreNameWidth = MeasureTextEx(font1, tempString, 27.0, 1.0).x;
                        sprintf(tempString,"%d",score->scoreValue);
                        float scoreValueWidth = MeasureTextEx(font1, tempString, 27.0, 1.0).x;
                        DrawTextEx(font1, tempString, (Vector2){404 - scoreValueWidth,y}, 27.0, 1.0, WHITE);
                        float lineY = y + 27.0 / 2.0f - 1.0f;
                        DrawLineEx((Vector2){75 + (scoreNameWidth + 10),lineY}, (Vector2){404 - (scoreValueWidth + 10),lineY}, 2, (Color){255,255,255,50});
                    } else {
                        sprintf(tempString,"%d)",i);
                        DrawTextEx(font1, tempString, (Vector2){66 - MeasureTextEx(font1, tempString, 27.0, 1.0).x,y}, 27.0, 1.0, GRAY);
                        DrawTextEx(font1, "No Score", (Vector2){75,y}, 27.0, 1.0, GRAY);
                    }
                    y += (27.0 * 0.8) + 2;
                }
            } else if (game.menuState == 1){
                DrawTexturePro(menuControls,(Rectangle){0,0,menuControls.width,menuControls.height},(Rectangle){26,320,menuControls.width/2,menuControls.height/2},(Vector2){0,0},0,WHITE);
            }

        }
        if (game.gameState == 1){
            // Game
            ClearBackground((Color){40,1,42,255});

            // Draw powerup status under game background
            float powerupProportion = game.powerupScoreDisplay / powerupTargetScore;
            if (powerupProportion > 1.0f){ powerupProportion = 1.0f; }
            float powerupHeight = (powerupEmptyY - powerupFullY) * 2;
            float powerupY = powerupFullY - (powerupProportion * powerupHeight / 2.0f);
            BeginShaderMode(swirlShader);
            DrawTexturePro(waterTex,(Rectangle){0,0,waterTex.width,waterTex.height},(Rectangle){30 * worldToScreen,powerupY* worldToScreen,powerupHeight* worldToScreen,powerupHeight* worldToScreen},(Vector2){0,0},0,WHITE);
            EndShaderMode();

            DrawTexturePro(bgTex,(Rectangle){0,0,bgTex.width,bgTex.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

            if (game.redPowerupOverlay > 0.0f){
                DrawTexturePro(redPowerupOverlay,(Rectangle){0,0,bgTex.width,bgTex.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,(Color){255,255,255,40.0*game.redPowerupOverlay});
            }

            // render bumpers which belong behind balls.
            for (int i = 0; i < numBumpers; i++){
                b2Vec2 pos = b2Body_GetPosition(bumpers[i].body);
                if (bumpers[i].type == 2 || bumpers[i].type == 3){
                    float width = 8.0f;
                    float height = 2.0f;
                    Color bumperColor = (Color){0,0,0,80};
                    if (bumpers[i].enabled == 0){
                        width = 8.0f;
                        height = 1.5f;
                    } else {
                        if (bumpers[i].type == 2){
                            bumperColor = RED;
                        } else if (bumpers[i].type == 3){
                            bumperColor = BLUE;
                        }
                    }
                    DrawTexturePro(bumper3,(Rectangle){0,0,bumper3.width,bumper3.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},bumpers[i].angle,bumperColor);
                }
            }

            // Render ball trails
            for (int i = 0; i < maxBalls; i++){
                if (balls[i].active == 1){
                    for (int ii = 1; ii <= 16; ii++){
                        int index = (balls[i].trailStartIndex + ii - 1);
                        if (index >= 16){ index -= 16; }
                        float trailSize = ballSize * sqrt(ii/16.0f);
                        Color ballColor = (Color){255,183,0,255};
                        if (balls[i].type == 1){ ballColor = BLUE; }
                        if (game.slowMotion == 1){ ballColor = WHITE; }
                        DrawTexturePro(trailTex,(Rectangle){0,0,trailTex.width,trailTex.height},(Rectangle){balls[i].locationHistoryX[index] * worldToScreen,balls[i].locationHistoryY[index] * worldToScreen,trailSize * worldToScreen,trailSize * worldToScreen},(Vector2){(trailSize / 2.0) * worldToScreen,(trailSize / 2.0) * worldToScreen},0,ballColor);

                    }
                }
            }

            //render balls
            for (int i = 0; i < maxBalls; i++){
                if (balls[i].active == 1){
                    b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                    Color ballColor = (Color){255,183,0,255};
                    if (balls[i].type == 1){ ballColor = BLUE; }
                    if (game.slowMotion == 1){ ballColor = WHITE; }
                    DrawTexturePro(ballTex,(Rectangle){0,0,ballTex.width,ballTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,ballSize * worldToScreen,ballSize * worldToScreen},(Vector2){(ballSize / 2.0) * worldToScreen,(ballSize / 2.0) * worldToScreen},0,ballColor);
                }
            }

            // Render bumpers which belong in front of balls
            for (int i = 0; i < numBumpers; i++){
                b2Vec2 pos = b2Body_GetPosition(bumpers[i].body);
                if (bumpers[i].type == 0){
                    float bounceScale = 0.2f;
                    float width = bumperSize + cos(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
                    float height = bumperSize + sin(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
                    float shockSize = (bumperSize * bumpers[i].bounceEffect) * 0.15f;
                    DrawTexturePro(shockwaveTex,(Rectangle){0,0,shockwaveTex.width,shockwaveTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){shockSize/2 * worldToScreen,shockSize/2 * worldToScreen},0,WHITE);
                    DrawTexturePro(bumperTex,(Rectangle){0,0,bumperTex.width,bumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},0,WHITE);
                } else if (bumpers[i].type == 1){
                    float width = 6.0f;
                    float height = 6.0f;
                    float shockPercent = (bumpers[i].bounceEffect) / 20.0f;
                    float shockSize = shockPercent * 20.0f;
                    float angle = sin(shaderSeconds) * 50.0f;
                    DrawTexturePro(iceBumperTex,(Rectangle){0,0,iceBumperTex.width,iceBumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,WHITE);
                    DrawTexturePro(trailTex,(Rectangle){0,0,trailTex.width,trailTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){(shockSize / 2.0) * worldToScreen,(shockSize / 2.0) * worldToScreen},0,(Color){255,255,255,255 * shockPercent});

                } else if (bumpers[i].type == 4){
                    float bounceScale = 0.2f;
                    float width = smallBumperSize + cos(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
                    float height = smallBumperSize + sin(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
                    width *= bumpers[i].enabledSize;
                    height *= bumpers[i].enabledSize;
                    float shockSize = (smallBumperSize * bumpers[i].bounceEffect) * 0.15f;
                    shockSize *= bumpers[i].enabledSize;
                    DrawTexturePro(shockwaveTex,(Rectangle){0,0,shockwaveTex.width,shockwaveTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){shockSize/2 * worldToScreen,shockSize/2 * worldToScreen},0,RED);
                    DrawTexturePro(bumperLightTex,(Rectangle){0,0,bumperTex.width,bumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},0,RED);
                    //printf("bumper %d: %f %f %f\n",i,width,height,bumpers[i].enabledSize);
                }
            }

            //render lower bumpers
            if (leftLowerBumperAnim > 0.0f){
                float percent = 1.0f - leftLowerBumperAnim;
                float x = 10.0f;
                float y = 117.2f;
                float width = 8.0f+ (2.0f * percent);
                float height = 18.0f + (4.0f * percent);
                float angle = -24.0f + sin(shaderSeconds * 100.0f) * 10.0f;
                DrawTexturePro(lowerBumperShock,(Rectangle){0,0,lowerBumperShock.width,lowerBumperShock.height},(Rectangle){x * worldToScreen,y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,(Color){255,255,255,255* (1.0f -percent)});
            }
            if (rightLowerBumperAnim > 0.0f){
                float percent = 1.0f - rightLowerBumperAnim;
                float x = 73.2f;
                float y = 117.2f;
                float width = 8.0f+ (2.0f * percent);
                float height = 18.0f + (4.0f * percent);
                float angle = 24.0f - sin(shaderSeconds * 100.0f) * 10.0f;
                DrawTexturePro(lowerBumperShock,(Rectangle){0,0,lowerBumperShock.width,lowerBumperShock.height},(Rectangle){x * worldToScreen,y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,(Color){255,255,255,255* (1.0f -percent)});

            }

            // Render left flipper
            b2Vec2 pos = b2Body_GetPosition(*leftFlipperBody);
            float angle = b2Rot_GetAngle(b2Body_GetRotation(*leftFlipperBody));
            DrawTexturePro(leftFlipperTex,(Rectangle){0,0,leftFlipperTex.width,leftFlipperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,flipperWidth * worldToScreen,flipperHeight * worldToScreen},(Vector2){0 * worldToScreen,0 * worldToScreen},(angle * RAD_TO_DEG),WHITE);

            // Render right flipper
            pos = b2Body_GetPosition(*rightFlipperBody);
            angle = b2Rot_GetAngle(b2Body_GetRotation(*rightFlipperBody));
            DrawTexturePro(rightFlipperTex,(Rectangle){0,0,rightFlipperTex.width,rightFlipperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,flipperWidth * worldToScreen,flipperHeight * worldToScreen},(Vector2){0 * worldToScreen,0 * worldToScreen},(angle * RAD_TO_DEG),WHITE);

            // Render score
            //sprintf(tempString,"%ld",game.gameScore);
            //DrawTextEx(font1, tempString, (Vector2){10,screenHeight - 35}, 30, 1.0, WHITE);

            // Render water powerup when active
            if (game.waterPowerupState > 0){
                float waterY = screenHeight * (1.0f - game.waterHeight);
                BeginShaderMode(swirlShader);
                DrawTexturePro(waterOverlayTex,(Rectangle){0,0,waterOverlayTex.width,waterOverlayTex.height},(Rectangle){0,waterY-30.0f,screenWidth,screenHeight},(Vector2){0,0},0,(Color){255,255,255,100});
                EndShaderMode();
            }

            if (game.bluePowerupOverlay > 0.0f){
                DrawRectangle(0,0,screenWidth,screenHeight,(Color){128,128,255,128*game.bluePowerupOverlay});
            }

            // Render ice powerup when active
            if (iceOverlayAlpha > 0.0f){
                BeginBlendMode(BLEND_ADDITIVE);
                DrawTexturePro(iceOverlay,(Rectangle){0,0,iceOverlay.width,iceOverlay.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,(Color){255,255,255,128*iceOverlayAlpha});
                EndBlendMode();
            }

            if (game.numBalls == 0 && game.numLives > 0){
                DrawRectangleRounded((Rectangle){108,600,screenWidth-238,80},0.1,16,(Color){0,0,0,100});
                DrawRectangleRounded((Rectangle){112,604,screenWidth-242,76},0.1,16,(Color){0,0,0,100});

                if (game.numLives == 5){
                    DrawTextEx(font1, "Ball 1 / 5", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Ball 1 / 5", 40.0, 1.0).x/2 - 10,610}, 40, 1.0, WHITE);
                } else if (game.numLives == 4){
                    DrawTextEx(font1, "Ball 2 / 5", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Ball 2 / 5", 40.0, 1.0).x/2 - 10,610}, 40, 1.0, WHITE);
                } else if (game.numLives == 3){
                    DrawTextEx(font1, "Ball 3 / 5", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Ball 3 / 5", 40.0, 1.0).x/2 - 10,610}, 40, 1.0, WHITE);
                } else if (game.numLives == 2){
                    DrawTextEx(font1, "Ball 4 / 5", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Ball 4 / 5", 40.0, 1.0).x/2 - 10,610}, 40, 1.0, WHITE);
                } else if (game.numLives == 1){
                    DrawTextEx(font1, "Ball 5 / 5", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Ball 5 / 5", 40.0, 1.0).x/2 - 10,610}, 40, 1.0, WHITE);
                }
                DrawTextEx(font1, "Center Button to Launch!", (Vector2){screenWidth/2 - MeasureTextEx(font1,  "Center Button to Launch!", 20.0, 1.0).x/2  - 10,650}, 20, 1.0, WHITE);

                for (int i = 0; i < 8; i++){
                    DrawTexturePro(arrowRight,(Rectangle){0,0,arrowRight.width,arrowRight.height},(Rectangle){screenWidth - 9,(i * 20) + 625+ (5 * sin(((i*100)+millis()-elapsedTimeStart)/200.0f)),20,20},(Vector2){16,16},-90,(Color){0,0,0,100});
                }
            }

            // Debug Rendering
            if (IsKeyDown(KEY_TAB)){
                DrawFPS(10, 10);

                DrawLine(0,mouseY,screenWidth,mouseY,RED);
                DrawLine(mouseX,0,mouseX,screenHeight,RED);
                if (IsMouseButtonPressed(0)){
                    printf("{%f,%f,,},\n",(float)(mouseX * screenToWorld),(float)(mouseY * screenToWorld));
                }

                // TODO: Implement Box2D debug draw
                // b2World_Draw(game.world, &drawOptions);
            }
        }
        if (game.gameState == 2){
            ClearBackground((Color){255,183,0,255});
            float timeFactor = (millis() - elapsedTimeStart) / 1000.0f;
            float xOffset = sin(timeFactor) * 50.0f;
            float yOffset = cos(timeFactor) * 50.0f;
            float angle = sin(timeFactor * 2) * 20 + cos(timeFactor / 3) * 25;
            float width = screenWidth * 3;
            float height = screenHeight * 3;
            float secondsVec[2] = { shaderSeconds, 0.0f };
            SetShaderValue(swirlShader, secondsLoc, secondsVec, SHADER_UNIFORM_VEC2);
			BeginShaderMode(swirlShader);
            DrawTexturePro(bgMenu,(Rectangle){0,0,bgMenu.width,bgMenu.height},(Rectangle){xOffset + screenWidth/2,yOffset + screenWidth/2,width,height},(Vector2){width/2,height/2},angle,WHITE);
            EndShaderMode();

            for (int i = 0; i < 16; i++){
                DrawTexturePro(ballTex,(Rectangle){0,0,ballTex.width,ballTex.height},(Rectangle){menuPinballs[i].px,menuPinballs[i].py,30,30},(Vector2){0,0},0,(Color){0,0,0,50});
            }

            DrawTexturePro(gameOverOverlay1,(Rectangle){0,0,gameOverOverlay1.width,gameOverOverlay1.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);
            DrawTexturePro(gameOverOverlay2,(Rectangle){0,0,gameOverOverlay2.width,gameOverOverlay2.height},(Rectangle){0,12 + sin((millis() - elapsedTimeStart) / 1000.0f)*5.0f,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

            sprintf(tempString,"%ld",game.gameScore);
            DrawTextEx(font2, "Score:", (Vector2){screenWidth/2 - MeasureTextEx(font2, "Score:", 60, 1.0).x/2,275}, 60, 1.0, WHITE);
            DrawTextEx(font2, tempString, (Vector2){screenWidth/2 - MeasureTextEx(font2, tempString, 60, 1.0).x/2,332}, 60, 1.0, WHITE);

            for (int i =0; i < 5; i++){
                sprintf(tempString,"%c",nameString[i]);
                float textWidth = MeasureTextEx(font2, tempString, 60, 1.0).x;
                if (nameString[i] == 32){
                    DrawTextEx(font2, "-", (Vector2){54 + (i * 62) - textWidth / 2,510}, 60, 1.0, DARKGRAY);
                } else {
                    DrawTextEx(font2, tempString, (Vector2){54 + (i * 62) - textWidth / 2,510}, 60, 1.0, WHITE);
                }
            }
            DrawTexturePro(arrowRight,(Rectangle){0,0,arrowRight.width,arrowRight.height},(Rectangle){54 + (game.nameSelectIndex * 62),595+ (5 * sin((millis()-elapsedTimeStart)/200.0f)),32,32},(Vector2){16,16},-90,WHITE);

        }
        if (game.gameState == 5){
            ClearBackground(WHITE);
        }

        if (game.transitionState > 0){
            float secondsVec[2] = { shaderSeconds, 0.0f };
            SetShaderValue(swirlShader, secondsLoc, secondsVec, SHADER_UNIFORM_VEC2);
            float transitionAmount = ((game.transitionAlpha / 255.0f));
            DrawRectanglePro((Rectangle){screenWidth,screenHeight,screenWidth,screenHeight + 200}, (Vector2){0,screenHeight + 200}, -33.0f * transitionAmount, BLACK);
            DrawRectanglePro((Rectangle){0,0,screenWidth,screenHeight + 200}, (Vector2){screenWidth,0}, -33.0f * transitionAmount, BLACK);
        }

        EndDrawing();
    }

    shutdownScores(scores);
    inputShutdown(input);
    shutdownSound(sound);
    physics_shutdown(&game);
    
    // Free allocated memory
    free(game.balls);
    free(menuPinballs);
    
    CloseWindow();

    return 0;
}
