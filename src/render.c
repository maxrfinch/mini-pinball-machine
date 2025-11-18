#include "render.h"
#include "constants.h"
#include <math.h>
#include <sys/time.h>

#define DEG_TO_RAD (3.14159265 / 180.0)
#define RAD_TO_DEG (180.0 / 3.14159265)

static long long millis() {
    struct timeval te;
    gettimeofday(&te, NULL);
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000;
    return milliseconds;
}

void Render_Gameplay(const GameStruct *game, const Resources *res,
                     const Bumper *bumpers, int numBumpers,
                     b2BodyId leftFlipperBody, b2BodyId rightFlipperBody,
                     float shaderSeconds, float iceOverlayAlpha,
                     int debugDrawEnabled, long long elapsedTimeStart) {
    
    const float ballSize = 5.0f;
    const float bumperSize = 10.0f;
    const float smallBumperSize = 4.0f;
    const int maxBalls = 256;
    
    ClearBackground((Color){40,1,42,255});

    // Draw powerup status under game background
    float powerupProportion = game->powerupScoreDisplay / 5000.0f;
    if (powerupProportion > 1.0f){ powerupProportion = 1.0f; }
    float powerupFullY = 64.0f;
    float powerupEmptyY = 104.4f;
    float powerupHeight = (powerupEmptyY - powerupFullY) * 2;
    float powerupY = powerupFullY - (powerupProportion * powerupHeight / 2.0f);
    BeginShaderMode(res->swirlShader);
    DrawTexturePro(res->waterTex,(Rectangle){0,0,res->waterTex.width,res->waterTex.height},(Rectangle){30 * worldToScreen,powerupY* worldToScreen,powerupHeight* worldToScreen,powerupHeight* worldToScreen},(Vector2){0,0},0,WHITE);
    EndShaderMode();

    DrawTexturePro(res->bgTex,(Rectangle){0,0,res->bgTex.width,res->bgTex.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,WHITE);

    if (game->redPowerupOverlay > 0.0f){
        DrawTexturePro(res->redPowerupOverlay,(Rectangle){0,0,res->bgTex.width,res->bgTex.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,(Color){255,255,255,40.0*game->redPowerupOverlay});
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
            DrawTexturePro(res->bumper3,(Rectangle){0,0,res->bumper3.width,res->bumper3.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},bumpers[i].angle,bumperColor);
        }
    }

    // Render ball trails
    Ball *balls = game->balls;
    for (int i = 0; i < maxBalls; i++){
        if (balls[i].active == 1){
            for (int ii = 1; ii <= 16; ii++){
                int index = (balls[i].trailStartIndex + ii - 1);
                if (index >= 16){ index -= 16; }
                float trailSize = ballSize * sqrt(ii/16.0f);
                Color ballColor = (Color){255,183,0,255};
                if (balls[i].type == 1){ ballColor = BLUE; }
                if (game->slowMotion == 1){ ballColor = WHITE; }
                DrawTexturePro(res->trailTex,(Rectangle){0,0,res->trailTex.width,res->trailTex.height},(Rectangle){balls[i].locationHistoryX[index] * worldToScreen,balls[i].locationHistoryY[index] * worldToScreen,trailSize * worldToScreen,trailSize * worldToScreen},(Vector2){(trailSize / 2.0) * worldToScreen,(trailSize / 2.0) * worldToScreen},0,ballColor);

            }
        }
    }

    //render balls
    for (int i = 0; i < maxBalls; i++){
        if (balls[i].active == 1){
            b2Vec2 pos = b2Body_GetPosition(balls[i].body);
            Color ballColor = (Color){255,183,0,255};
            if (balls[i].type == 1){ ballColor = BLUE; }
            if (game->slowMotion == 1){ ballColor = WHITE; }
            DrawTexturePro(res->ballTex,(Rectangle){0,0,res->ballTex.width,res->ballTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,ballSize * worldToScreen,ballSize * worldToScreen},(Vector2){(ballSize / 2.0) * worldToScreen,(ballSize / 2.0) * worldToScreen},0,ballColor);
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
            DrawTexturePro(res->shockwaveTex,(Rectangle){0,0,res->shockwaveTex.width,res->shockwaveTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){shockSize/2 * worldToScreen,shockSize/2 * worldToScreen},0,WHITE);
            DrawTexturePro(res->bumperTex,(Rectangle){0,0,res->bumperTex.width,res->bumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},0,WHITE);
        } else if (bumpers[i].type == 1){
            // Ice bumper (slow-mo powerup)
            float width = 6.0f;
            float height = 6.0f;
            float shockPercent = (bumpers[i].bounceEffect) / 20.0f;
            float shockSize = shockPercent * 20.0f;
            float angle = sin(shaderSeconds) * 50.0f;
            
            // Calculate bumper alpha based on powerup state
            int bumperAlpha = 255;
            if (game->slowMoPowerupAvailable == 0) {
                // Powerup unavailable - invisible (but physics still active)
                bumperAlpha = 0;
            } else {
                // Powerup available - blink to indicate ready
                // Use shaderSeconds for periodic blinking (0.5-1.0 range)
                float blinkValue = 0.75f + 0.25f * sin(shaderSeconds * 4.0f);
                bumperAlpha = (int)(255 * blinkValue);
            }
            
            // Draw ice bumper with calculated alpha
            DrawTexturePro(res->iceBumperTex,(Rectangle){0,0,res->iceBumperTex.width,res->iceBumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,(Color){255,255,255,bumperAlpha});
            
            // Only draw visual effects when powerup is available or explosion is active
            if (game->slowMoPowerupAvailable == 1 || game->slowMoExplosionEffect > 0.0f) {
                // Draw regular bounce effect
                if (bumpers[i].bounceEffect > 0.0f) {
                    DrawTexturePro(res->trailTex,(Rectangle){0,0,res->trailTex.width,res->trailTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){(shockSize / 2.0) * worldToScreen,(shockSize / 2.0) * worldToScreen},0,(Color){255,255,255,255 * shockPercent});
                }
                
                // Draw explosion effect when triggered
                if (game->slowMoExplosionEffect > 0.0f) {
                    float explosionSize = 25.0f * (1.0f - game->slowMoExplosionEffect);
                    int explosionAlpha = (int)(255 * game->slowMoExplosionEffect);
                    DrawTexturePro(res->shockwaveTex,(Rectangle){0,0,res->shockwaveTex.width,res->shockwaveTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,explosionSize * worldToScreen,explosionSize * worldToScreen},(Vector2){(explosionSize / 2.0) * worldToScreen,(explosionSize / 2.0) * worldToScreen},0,(Color){255,255,255,explosionAlpha});
                }
            }

        } else if (bumpers[i].type == 4){
            float bounceScale = 0.2f;
            float width = smallBumperSize + cos(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
            float height = smallBumperSize + sin(millis() / 20.0) * bumpers[i].bounceEffect * bounceScale;
            width *= bumpers[i].enabledSize;
            height *= bumpers[i].enabledSize;
            float shockSize = (smallBumperSize * bumpers[i].bounceEffect) * 0.15f;
            shockSize *= bumpers[i].enabledSize;
            DrawTexturePro(res->shockwaveTex,(Rectangle){0,0,res->shockwaveTex.width,res->shockwaveTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,shockSize * worldToScreen,shockSize * worldToScreen},(Vector2){shockSize/2 * worldToScreen,shockSize/2 * worldToScreen},0,RED);
            DrawTexturePro(res->bumperLightTex,(Rectangle){0,0,res->bumperTex.width,res->bumperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},0,RED);
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
        DrawTexturePro(res->lowerBumperShock,(Rectangle){0,0,res->lowerBumperShock.width,res->lowerBumperShock.height},(Rectangle){x * worldToScreen,y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,(Color){255,255,255,255* (1.0f -percent)});
    }
    if (rightLowerBumperAnim > 0.0f){
        float percent = 1.0f - rightLowerBumperAnim;
        float x = 73.2f;
        float y = 117.2f;
        float width = 8.0f+ (2.0f * percent);
        float height = 18.0f + (4.0f * percent);
        float angle = 24.0f - sin(shaderSeconds * 100.0f) * 10.0f;
        DrawTexturePro(res->lowerBumperShock,(Rectangle){0,0,res->lowerBumperShock.width,res->lowerBumperShock.height},(Rectangle){x * worldToScreen,y * worldToScreen,width * worldToScreen,height * worldToScreen},(Vector2){(width / 2.0) * worldToScreen,(height / 2.0) * worldToScreen},angle,(Color){255,255,255,255* (1.0f -percent)});

    }

    // Render left flipper
    b2Vec2 pos = b2Body_GetPosition(leftFlipperBody);
    float angle = b2Rot_GetAngle(b2Body_GetRotation(leftFlipperBody));
    // The collision shape is offset by (-flipperHeight/2, -flipperHeight/2) in local space
    // So the texture origin (pivot) should be at (flipperHeight/2, flipperHeight/2) to match
    DrawTexturePro(res->leftFlipperTex,(Rectangle){0,0,res->leftFlipperTex.width,res->leftFlipperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,flipperWidth * worldToScreen,flipperHeight * worldToScreen},(Vector2){(flipperHeight / 2.0f) * worldToScreen,(flipperHeight / 2.0f) * worldToScreen},(angle * RAD_TO_DEG),WHITE);

    // Render right flipper
    pos = b2Body_GetPosition(rightFlipperBody);
    angle = b2Rot_GetAngle(b2Body_GetRotation(rightFlipperBody));
    // The collision shape is offset by (-flipperHeight/2, -flipperHeight/2) in local space
    // So the texture origin (pivot) should be at (flipperHeight/2, flipperHeight/2) to match
    DrawTexturePro(res->rightFlipperTex,(Rectangle){0,0,res->rightFlipperTex.width,res->rightFlipperTex.height},(Rectangle){pos.x * worldToScreen,pos.y * worldToScreen,flipperWidth * worldToScreen,flipperHeight * worldToScreen},(Vector2){(flipperHeight / 2.0f) * worldToScreen,(flipperHeight / 2.0f) * worldToScreen},(angle * RAD_TO_DEG),WHITE);

    // Render water powerup when active
    if (game->waterPowerupState > 0){
        float baseWaterY = screenHeight * (1.0f - game->waterHeight);

        // Subtle bob
        float verticalOffset = sinf(shaderSeconds * 4.0f) * 8.0f;
        float waterY = baseWaterY + verticalOffset;

        // Stronger dynamic ripples (true wave motion)
        float rippleX = sinf(shaderSeconds * 6.0f) * 12.0f;
        float rippleY = cosf(shaderSeconds * 3.0f) * 6.0f;

        Rectangle src = (Rectangle){
            rippleX, rippleY,
            (float)res->waterOverlayTex.width,
            (float)res->waterOverlayTex.height
        };

        Rectangle dst = (Rectangle){
            0.0f,
            waterY - 40.0f,
            (float)screenWidth,
            (float)screenHeight
        };

        Vector2 origin = (Vector2){ 0.0f, 0.0f };
        Color tint = (Color){ 255, 255, 255, 120 };

        // Enable water shader with ripple effects
        BeginShaderMode(res->waterShader);
        DrawTexturePro(res->waterOverlayTex, src, dst, origin, 0.0f, tint);
        EndShaderMode();
    }

    if (game->bluePowerupOverlay > 0.0f){
        DrawRectangle(0,0,screenWidth,screenHeight,(Color){128,128,255,128*game->bluePowerupOverlay});
    }

    // Render ice powerup when active
    if (iceOverlayAlpha > 0.0f){
        BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(res->iceOverlay,(Rectangle){0,0,res->iceOverlay.width,res->iceOverlay.height},(Rectangle){0,0,screenWidth,screenHeight},(Vector2){0,0},0,(Color){255,255,255,128*iceOverlayAlpha});
        EndBlendMode();
    }

    if (game->numBalls == 0 && game->numLives > 0){
        DrawRectangleRounded((Rectangle){108,600,screenWidth-238,80},0.1,16,(Color){0,0,0,100});
        DrawRectangleRounded((Rectangle){112,604,screenWidth-242,76},0.1,16,(Color){0,0,0,100});

        const int totalBalls = 3;
        int currentBall = totalBalls - game->numLives + 1;
        if (currentBall < 1) currentBall = 1;
        if (currentBall > totalBalls) currentBall = totalBalls;

        char ballText[32];
        snprintf(ballText, sizeof(ballText), "Ball %d / %d", currentBall, totalBalls);

        DrawTextEx(
            res->font1,
            ballText,
            (Vector2){
                screenWidth/2 - MeasureTextEx(res->font1, ballText, 40.0, 1.0).x/2 - 10,
                610
            },
            40,
            1.0,
            WHITE
        );
        DrawTextEx(res->font1, "Center Button to Launch!", (Vector2){screenWidth/2 - MeasureTextEx(res->font1,  "Center Button to Launch!", 20.0, 1.0).x/2  - 10,650}, 20, 1.0, WHITE);

        for (int i = 0; i < 8; i++){
            DrawTexturePro(res->arrowRight,(Rectangle){0,0,res->arrowRight.width,res->arrowRight.height},(Rectangle){screenWidth - 9,(i * 20) + 625+ (5 * sin(((i*100)+millis()-elapsedTimeStart)/200.0f)),20,20},(Vector2){16,16},-90,(Color){0,0,0,100});
        }
    }

    // Debug Rendering
    if (IsKeyDown(KEY_TAB)){
        DrawFPS(10, 10);

        float mouseX = GetMouseX();
        float mouseY = GetMouseY();
        DrawLine(0,mouseY,screenWidth,mouseY,RED);
        DrawLine(mouseX,0,mouseX,screenHeight,RED);
    }

    // Draw physics debug visualization if enabled
    if (debugDrawEnabled) {
        physics_debug_draw(game);
    }
}
