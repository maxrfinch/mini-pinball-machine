#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
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
#include "util.h"
#include "game.h"
#include "water.h"
#include "powerups.h"
#include "menu.h"

#if defined(PLATFORM_RPI)
    #define GLSL_VERSION            100
#else   // PLATFORM_RPI, PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            330
#endif

// global pointer to the bumpers array
Bumper* bumpers = NULL;

// Global water system instance
static WaterSystem waterSystem;

// AddWaterImpulse: Called from physics.c when ball hits water
void AddWaterImpulse(float x, float impulse) {
    Water_AddImpulse(&waterSystem, x, impulse);
    waterSystem.impactIntensity += 0.6f;
    if (waterSystem.impactIntensity > 1.5f) {
        waterSystem.impactIntensity = 1.5f;
    }
}

int main(void){

    // Initialize a struct encoding data about the game.
    GameStruct game;
    game.gameState = 0;


    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Mini Pinball!");
    SetTargetFPS(60);

    // Go fullscreen
    ToggleFullscreen();

    // Render-to-texture target at virtual resolution (600 x 1024)
    RenderTexture2D gameTarget = LoadRenderTexture(screenWidth, screenHeight);
    // Keep pixels crisp; use TEXTURE_FILTER_BILINEAR if you want smoothing
    SetTextureFilter(gameTarget.texture, TEXTURE_FILTER_POINT);

    SoundManager *sound = initSound();
    game.sound = sound;

    // Initialize all resources (textures, shaders, fonts)
    Resources resources;
    Resources_Init(&resources);

    // Initialize water system
    Water_Init(&waterSystem);
    
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

    char nameString[6];
    sprintf(nameString,"     ");

    // Initialize game state machine
    Game_Init(&game, bumpers);
    
    // Initialize flippers
    physics_flippers_init(&game, leftFlipperBody, rightFlipperBody);
    
    // Initialize powerup system
    PowerupSystem powerupSystem;
    Powerups_Init(&game, &powerupSystem);
    
    // Initialize menu
    Menu_Init(&game, menuPinballs, 32);

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

        // Update water simulation and shader uniforms
        Water_Update(&waterSystem, &resources, GetFrameTime());
        
        // Drive ripple amplitude based on water impact intensity
        float ampScale = 1.0f + 2.5f * waterSystem.impactIntensity;
        float ampXVecCurrent[2] = { ampX * ampScale, 0.0f };
        float ampYVecCurrent[2] = { ampY * ampScale, 0.0f };
        SetShaderValue(resources.swirlShader, resources.swirlAmpXLoc, ampXVecCurrent, SHADER_UNIFORM_VEC2);
        SetShaderValue(resources.swirlShader, resources.swirlAmpYLoc, ampYVecCurrent, SHADER_UNIFORM_VEC2);

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

        // STEP SIMULATION AT FIXED RATE with safety cap
        const int MAX_PHYSICS_STEPS_PER_FRAME = 16;
        int stepCount = 0;
        while (accumulatedTime > timestep && stepCount < MAX_PHYSICS_STEPS_PER_FRAME){
            accumulatedTime -= timestep;
            stepCount++;

            updateSound(sound,&game);

            // Update game state machine and transitions
            Game_Update(&game, bumpers, input, scores, sound, timeStep);

            // Update menu if in menu state
            if (game.gameState == 0){
                Menu_Update(&game, menuPinballs, 32, input, sound);
            }
            
            // Update powerup system
            float effectiveTimestep = (timeStep) * powerupSystem.slowMotionFactor;
            Powerups_Update(&game, &powerupSystem, input, sound, effectiveTimestep);
            if (game.gameState == 1){
                // Game

                // Clamp effective timestep to a sane, non-zero range to avoid numerical issues.
                if (effectiveTimestep < (1.0f / 600.0f)) {
                    effectiveTimestep = 1.0f / 600.0f;
                }
                if (effectiveTimestep > (1.0f / 20.0f)) {
                    effectiveTimestep = 1.0f / 20.0f;
                }
                
                // Update flippers
                float deltaAngularVelocityLeft = 0.0f;
                float deltaAngularVelocityRight = 0.0f;
                physics_flippers_update(&game, leftFlipperBody, rightFlipperBody, input, sound, 
                                       effectiveTimestep, &deltaAngularVelocityLeft, &deltaAngularVelocityRight);

                physics_step(&game, effectiveTimestep);

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
                        // Send BALL_READY event once when no balls and lives remain
                        if (game.ballReadyEventSent == 0) {
                            inputSendBallReady(input);
                            game.ballReadyEventSent = 1;
                        }
                        
                        // Launch ball on center button press
                        if (inputCenterPressed(input)){
                            physics_add_ball(&game,89.5 - ballSize / 2,160,0,-220,0);
                            // Send BALL_LAUNCHED event - Pico handles 5x strobe animation
                            inputSendBallLaunched(input);
                            game.ballReadyEventSent = 0;  // Reset for next time
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

                // Update water height based on powerup state
                if (game.waterPowerupState == 1){
                    game.waterHeight += 0.006f * powerupSystem.slowMotionFactor;
                    if (game.waterHeight > game.waterHeightTarget){
                        game.waterHeight = game.waterHeightTarget;
                    }
                } else if (game.waterPowerupState == 2) {
                    game.waterHeight -= 0.0005f * powerupSystem.slowMotionFactor;
                    if (game.waterHeight < 0.0f){
                        game.waterHeight = 0.0f;
                        game.waterPowerupState = 0;
                    }
                }

                if (game.waterHeightTimer > 0.0f){
                    game.waterHeightTimer -= 1.0f * powerupSystem.slowMotionFactor;
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
                                    waterSystem.impactIntensity += 0.6f;
                                    if (waterSystem.impactIntensity > 1.5f) {
                                        waterSystem.impactIntensity = 1.5f;
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
                // Game over - delegate to scoreboard update
                Scoreboard_Update(&game, input, scores, nameString);
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
        // 1) Draw the game into the virtual 600x1024 canvas
        BeginTextureMode(gameTarget);
        ClearBackground(BLACK);   // or whatever your default background is

        if (game.gameState == 0){
            // Menu
            UI_DrawMenu(&game, &resources, menuPinballs, 16, scores, elapsedTimeStart, shaderSeconds);
        }
        if (game.gameState == 1){
            // Game
            Render_Gameplay(&game, &resources, bumpers, numBumpers, 
                        *leftFlipperBody, *rightFlipperBody,
                        shaderSeconds, powerupSystem.iceOverlayAlpha, 
                        debugDrawEnabled, elapsedTimeStart);
        }
        if (game.gameState == 2){
            // Game Over
            UI_DrawGameOver(&game, &resources, menuPinballs, 16, nameString, elapsedTimeStart, shaderSeconds);
        }
        if (game.gameState == 5){
            ClearBackground(WHITE);
        }

        // Draw transition overlay if active (still in virtual space)
        UI_DrawTransition(&game, shaderSeconds);

        EndTextureMode();

        // 2) Now scale that canvas to whatever the real fullscreen size is
        BeginDrawing();
        ClearBackground(BLACK);   // letterbox bars color

        int renderW = GetRenderWidth();
        int renderH = GetRenderHeight();

        // Compute uniform scale so the whole 600x1024 fits without cropping
        float scaleX = (float)renderW / (float)screenWidth;
        float scaleY = (float)renderH / (float)screenHeight;
        float scale = (scaleX < scaleY) ? scaleX : scaleY;

        float drawW = (float)screenWidth  * scale;
        float drawH = (float)screenHeight * scale;

        // Center on screen (letterboxing)
        float offsetX = (renderW - drawW) * 0.5f;
        float offsetY = (renderH - drawH) * 0.5f;

        // Draw the render texture to screen.
        // Note: src.height is NEGATIVE to flip the texture vertically (raylib quirk).
        DrawTexturePro(
            gameTarget.texture,
            (Rectangle){ 0, 0, (float)screenWidth, -(float)screenHeight },
            (Rectangle){ offsetX, offsetY, drawW, drawH },
            (Vector2){ 0, 0 },
            0.0f,
            WHITE
        );

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