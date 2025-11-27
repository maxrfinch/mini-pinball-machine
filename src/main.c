#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <box2d/box2d.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
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

void redirect_logs_to_file(void) {
    
    // Get home directory
    const char *home = getenv("HOME");
    if (!home) home = "/home/pi";  // Fallback
    
    char log_path[256];
    snprintf(log_path, sizeof(log_path), "%s/Desktop/pinball_log.txt", home);
    
    // Open log file (create if doesn't exist, append if it does)
    int log_fd = open(log_path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (log_fd == -1) {
        perror("Failed to open log file");
        return;
    }
    
    // Redirect stdout and stderr to the log file
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);
    close(log_fd);
    
    // Make output unbuffered so logs appear immediately
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    printf("=== Pinball Machine Log Started ===\n");
    fflush(stdout);
}

int main(void){
    redirect_logs_to_file();

    // Initialize a struct encoding data about the game.
    GameStruct game;
    game.gameState = 0;
    game.waterHeight = 0.0f;           // ADD THIS LINE
    game.waterHeightTarget = 0.0f;     // ADD THIS LINE
    game.waterPowerupState = 0;        // ADD THIS LINE
    game.waterHeightTimer = 0.0f;     
    game.bumpers = NULL;  // Initialize to NULL before physics_init


    SetConfigFlags(FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "Mini Pinball!");
    SetTargetFPS(60);
    
    // Go fullscreen only on Raspberry Pi
    #if defined(PLATFORM_RPI)
        ToggleFullscreen();
    #endif

    // Render-to-texture target at virtual resolution (600 x 1024)
    RenderTexture2D gameTarget = LoadRenderTexture(screenWidth, screenHeight);
    // Keep pixels crisp; use TEXTURE_FILTER_BILINEAR if you want smoothing
    SetTextureFilter(gameTarget.texture, TEXTURE_FILTER_BILINEAR);

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
    if (balls == NULL) {
        TraceLog(LOG_ERROR, "Failed to allocate memory for balls array");
        physics_shutdown(&game);
        shutdownSound(sound);
        Resources_Unload(&resources);
        CloseWindow();
        return 1;
    }
    game.balls = balls;
    game.numBalls = 0;
    for (int i = 0; i < maxBalls; i++){
        balls[i].active = 0;
    }

    // Setup render texture for special ball effect
    RenderTexture2D renderTarget = LoadRenderTexture(screenWidth, screenHeight);

    // Menu setup
    MenuPinball* menuPinballs = malloc(32 * sizeof(MenuPinball));
    if (menuPinballs == NULL) {
        TraceLog(LOG_ERROR, "Failed to allocate memory for menu pinballs array");
        free(game.balls);
        physics_shutdown(&game);
        shutdownSound(sound);
        Resources_Unload(&resources);
        CloseWindow();
        return 1;
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
    
    // Initialize camera system
    Camera_Init(&game.camera);
    
    // Initialize name entry state
    game.centerHeldCounter = 0;

    // Initialize launch charge state
    game.launchCharging = 0;
    game.launchChargeAmount = 0.0f;
    game.launchChargeTime = 0.0f;

    inputSetGameState(input,STATE_MENU);
    TraceLog(LOG_INFO, "START");

    int lastGameState = game.gameState;

    // Debug draw toggle state
    int debugDrawEnabled = 0;
    const int MAX_PHYSICS_STEPS_PER_FRAME = 16;
    int stepCount = 0;

    // Initialize quit request flag
    game.quitRequested = 0;

    while (!WindowShouldClose() && !game.quitRequested){
        int prevGameState = lastGameState;

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
        stepCount = 0;
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

                if (game. oldGameScore != game.gameScore) {
                    inputSetScore(input, game.gameScore);
                    game.oldGameScore = game.gameScore;
                    
                    // Pre-warm camera if score enters top 3 and camera not already active
                    if (!game.camera.preview_active && game.camera.initialized) {
                        if (isScoreInTopN(scores, game.gameScore, 3)) {
                            Camera_StartPreview(&game.camera);
                            TraceLog(LOG_INFO, "CAMERA: Pre-warming preview (score entered top 3)");
                        }
                    }
                }
                
                // Check and handle powerup state machines
                Powerups_CheckMultiball(&game, bumpers, sound, input);
                Powerups_CheckBumperPowerup(&game, bumpers, sound);

                if (game.numBalls == 0){
                    if (game.numLives >= 1){
                        // Send ball ready signal to controller if not already sent
                        if (game.ballReadyEventSent == 0){
                            inputSendBallReady(input);
                            game.ballReadyEventSent = 1;
                        }
                        
                        // Charge-up mechanic for ball launch
                        if (inputCenter(input)){
                            // Center button is held - charge up
                            if (game.launchCharging == 0){
                                // Just started charging
                                game.launchCharging = 1;
                                game.launchChargeTime = 0.0f;
                                game.launchChargeAmount = 0.0f;
                            }
                            
                            // Update charge amount (charge over 1.5 seconds)
                            const float chargeTime = 1.5f;
                            game.launchChargeTime += timeStep;
                            game.launchChargeAmount = game.launchChargeTime / chargeTime;
                            if (game.launchChargeAmount > 1.0f){
                                game.launchChargeAmount = 1.0f;
                            }
                            
                            // Send charge status to firmware every frame
                            int chargePercent = (int)(game.launchChargeAmount * 100.0f);
                            inputSendChargeStatus(input, chargePercent);
                            
                            // Send haptic strength update (stub for future firmware haptic hardware)
                            inputSendHapticStrength(input, chargePercent);
                        } else {
                            // Center button released - launch ball if was charging
                            if (game.launchCharging == 1){
                                // Calculate launch velocity based on charge
                                // Minimum velocity: -220 (no charge)
                                // Maximum velocity: -400 (full charge)
                                float minVel = -220.0f;
                                float maxVel = -400.0f;
                                float launchVel = minVel + (maxVel - minVel) * game.launchChargeAmount;
                                
                                Game_SpawnBall(&game, 89.5 - ballSize / 2, 160, 0, launchVel, 0);
                                
                                // Play launch haptic effect
                                sound_play_haptic_launch(sound);
                                
                                // Send ball launched signal to controller
                                inputSendBallLaunched(input);
                                game.ballReadyEventSent = 0;  // Reset for next ball
                                
                                // Clear charge display
                                inputSendChargeStatus(input, 0);
                                
                                // Reset charge state
                                game.launchCharging = 0;
                                game.launchChargeAmount = 0.0f;
                                game.launchChargeTime = 0.0f;
                            }
                        }
                    } else {
                        // game over condition
                        if (game.transitionState == 0){
                            game.transitionState = 1;
                            // Check if score is in top 10 to determine which screen to show
                            if (isScoreInTopN(scores, game.gameScore, 3)) {
                                // Top 3 score - show special game over screen with photo capture
                                game.transitionTarget = TRANSITION_GAME_OVER_TOP3;
                                inputSetGameState(input,STATE_GAME_OVER);
                                inputSendHighScoreEffect(input);
                                // Reset nameString for fresh name entry
                                sprintf(nameString, "     ");
                                game.centerHeldCounter = 0;
                            } else if (isScoreInTopN(scores, game.gameScore, 10)) {
                                // Top 10 score - show regular game over screen for name entry
                                game.transitionTarget = TRANSITION_GAME_OVER;
                                inputSetGameState(input,STATE_GAME_OVER);
                                inputSendHighScoreEffect(input);
                                // Reset nameString for fresh name entry
                                sprintf(nameString, "     ");
                                game.centerHeldCounter = 0;
                            } else {
                                // Score not in top 10 - go directly to main menu
                                game.transitionTarget = TRANSITION_TO_MENU;
                                inputSetGameState(input,STATE_MENU);
                                inputSendGameOverCurtainEffect(input);
                            }
                        }
                    }
                }

                if (IsMouseButtonPressed(0)){
                    Game_SpawnBall(&game, (mouseX) * screenToWorld, (mouseY) * screenToWorld, 0, 0, 1);
                }

                // Check if any balls have fallen outside the screen
                // Remove them if they have.
                // Check if any balls are standing still for too long and remove.
                // Note: We iterate all maxBalls slots since active balls may be sparse.
                // Caching game.numBalls isn't beneficial here because we may modify it mid-loop.
                for (int i = 0; i < maxBalls; i++){
                    if (balls[i].active != 1){
                        continue; // Skip inactive balls early
                    }
                    // Cache position and velocity for this ball - values don't change mid-iteration
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
                        int ballType = balls[i].type;  // Preserve ball type
                        balls[i].active = 0;
                        b2DestroyBody(balls[i].body);
                        game.numBalls--;
                        
                        // Check if this is a stuck ball (not one that fell off screen)
                        if (balls[i].killCounter > 100) {
                            // Respawn stuck ball from shooter
                            // Game_SpawnBall increments numBalls, so net change is 0
                            Game_SpawnBall(&game, 89.5 - ballSize / 2, 160, 0, -220, ballType);
                        } else {
                            // Ball fell off screen normally - check for game over
                            if (game.numBalls == 0 && game.numLives >= 1){
                                game.numLives -= 1;
                                inputSetNumBalls(input,game.numLives);
                            }
                        }
                    }
                }

                //Update ball trails
                // Note: This bounds check fixes the game state (unlike render.c which uses a local copy)
                for (int i = 0; i < maxBalls; i++){
                    if (balls[i].active != 1){
                        continue; // Skip inactive balls early
                    }
                    // Bounds check and fix trailStartIndex before array access
                    if (balls[i].trailStartIndex < 0 || balls[i].trailStartIndex >= TRAIL_HISTORY_SIZE) {
                        TraceLog(LOG_WARNING, "Ball %d trailStartIndex out of bounds: %d, clamping to 0", i, balls[i].trailStartIndex);
                        balls[i].trailStartIndex = 0;
                    }
                    b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                    balls[i].locationHistoryX[balls[i].trailStartIndex] = pos.x;
                    balls[i].locationHistoryY[balls[i].trailStartIndex] = pos.y;
                    balls[i].trailStartIndex = (balls[i].trailStartIndex + 1) % TRAIL_HISTORY_SIZE;
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
                        // End water effect, return to in-game visuals
                        inputSendWaterEffectEnd(input);
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
                    // Cache water Y position - doesn't change during loop iteration
                    float waterY = worldHeight * (1.0f - game.waterHeight);

                    for (int i = 0; i < maxBalls; i++){
                        if (balls[i].active != 1){
                            continue; // Skip inactive balls early
                        }
                        // Cache position and velocity for this ball - used multiple times in this iteration
                        b2Vec2 pos = b2Body_GetPosition(balls[i].body);
                        b2Vec2 vel = b2Body_GetLinearVelocity(balls[i].body);
                        if (pos.y > waterY){
                            float distUnderwater = fabs(waterY - pos.y);
                            float bVely = -200.0f + -(distUnderwater * 40.0f);
                            b2Vec2 force = {0, bVely};
                            b2Body_ApplyForceToCenter(balls[i].body, force, true);
                            // Apply special forces for flipper
                            float flipperForce = -1000.0f;
                            if (pos.x <= worldWidth / 2.0f && fabsf(deltaAngularVelocityLeft) > 0.0f){
                                b2Vec2 flipForce = {0, flipperForce};
                                b2Body_ApplyForceToCenter(balls[i].body, flipForce, true);
                            }
                            if (pos.x >= worldWidth / 2.0f && fabsf(deltaAngularVelocityRight) > 0.0f){
                                b2Vec2 flipForce = {0, flipperForce};
                                b2Body_ApplyForceToCenter(balls[i].body, flipForce, true);
                            }
                        }
                    }

                }
            }
            if (game.gameState == 2 || game.gameState == 3){
                // Game over (both regular and top 3) - delegate to scoreboard update
                Scoreboard_Update(&game, input, scores, nameString);
            }
        }

        // Check if physics fell behind and clamp accumulated time
        if (stepCount == MAX_PHYSICS_STEPS_PER_FRAME && accumulatedTime > timestep) {
            TraceLog(LOG_WARNING,
                     "Physics fell behind: accumulatedTime=%lld, clamping",
                     accumulatedTime);
            // Keep some accumulated time to prevent complete physics loss during
            // temporary lag spikes. This allows partial catch-up next frame.
            accumulatedTime = timestep * 2;
        }

        // If the high-level game state changed this frame, notify the Pico so it can
        // update button LED baselines (menu, gameplay, game over patterns).
        if (game.gameState != prevGameState) {
            switch (game.gameState) {
                case 0:
                    // Menu state
                    inputSetGameState(input, STATE_MENU);
                    break;
                case 1:
                    // Gameplay state
                    inputSetGameState(input, STATE_GAME);
                    break;
                case 2:
                    // Game over / scoreboard state
                    inputSetGameState(input, STATE_GAME_OVER);
                    break;
                case 3:
                    // Game over top 3 / scoreboard state
                    inputSetGameState(input, STATE_GAME_OVER);
                    break;
                default:
                    break;
            }
            lastGameState = game.gameState;
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
        if (game.gameState == 3){
            // Game Over - Top 3
            UI_DrawGameOverTop3(&game, &resources, menuPinballs, 16, nameString, scores, elapsedTimeStart, shaderSeconds);
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

        #if defined(PLATFORM_RPI)
            int renderW = GetRenderWidth();
            int renderH = GetRenderHeight();
        #else
            int renderW = GetScreenWidth();
            int renderH = GetScreenHeight();
        #endif

        

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
    
    // Shutdown camera system
    Camera_Shutdown(&game.camera);
    
    // Unload all resources
    Resources_Unload(&resources);
    
    // Free allocated memory
    free(game.balls);
    free(menuPinballs);
    
    CloseWindow();

    return 0;
}