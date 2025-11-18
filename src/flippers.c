#include "flippers.h"
#include "constants.h"
#include <math.h>

#define DEG_TO_RAD (3.14159265 / 180.0)
#define RAD_TO_DEG (180.0 / 3.14159265)

// Module-level state for flipper angles
static float leftFlipperAngle = 0.0f;
static float rightFlipperAngle = 0.0f;
static float flipperSpeedScalar = 1.0f;

void Flippers_Init(GameStruct *game, b2BodyId *leftFlipperBody, b2BodyId *rightFlipperBody) {
    leftFlipperAngle = flipperRestAngleLeft;
    rightFlipperAngle = flipperRestAngleRight;
    game->leftFlipperState = 0;
    game->rightFlipperState = 0;
}

void Flippers_Update(GameStruct *game,
                     b2BodyId *leftFlipperBody,
                     b2BodyId *rightFlipperBody,
                     InputManager *input,
                     SoundManager *sound,
                     float dt,
                     float *out_leftDeltaAngularVelocity,
                     float *out_rightDeltaAngularVelocity) {
    
    float oldAngleLeft = leftFlipperAngle;
    float oldAngleRight = rightFlipperAngle;
    float targetAngleLeft = 0.0f;
    float targetAngleRight = 0.0f;

    // Left flipper
    if (inputLeft(input)) {
        if (game->leftFlipperState == 0) {
            playFlipper(sound);
            game->leftFlipperState = 1;
        }
        targetAngleLeft = flipperActiveAngleLeft;
        leftFlipperAngle -= (flipperSpeed * dt);
        if (leftFlipperAngle < targetAngleLeft) {
            leftFlipperAngle = targetAngleLeft;
        }
    } else {
        if (game->leftFlipperState == 1) {
            playFlipper(sound);
        }
        game->leftFlipperState = 0;
        targetAngleLeft = flipperRestAngleLeft;
        leftFlipperAngle += (flipperSpeed * dt);
        if (leftFlipperAngle > targetAngleLeft) {
            leftFlipperAngle = targetAngleLeft;
        }
    }

    // Right flipper
    if (inputRight(input)) {
        if (game->rightFlipperState == 0) {
            playFlipper(sound);
            game->rightFlipperState = 1;
        }
        targetAngleRight = flipperActiveAngleRight;
        rightFlipperAngle += (flipperSpeed * dt);
        if (rightFlipperAngle > targetAngleRight) {
            rightFlipperAngle = targetAngleRight;
        }
    } else {
        if (game->rightFlipperState == 1) {
            playFlipper(sound);
        }
        game->rightFlipperState = 0;
        targetAngleRight = flipperRestAngleRight;
        rightFlipperAngle -= (flipperSpeed * dt);
        if (rightFlipperAngle < targetAngleRight) {
            rightFlipperAngle = targetAngleRight;
        }
    }

    // Calculate delta angular velocities
    float deltaAngularVelocityLeft = 0.0f;
    float deltaAngularVelocityRight = 0.0f;
    if (dt > 0.0f) {
        deltaAngularVelocityLeft = ((leftFlipperAngle * DEG_TO_RAD) - (oldAngleLeft * DEG_TO_RAD)) / dt;
        deltaAngularVelocityRight = ((rightFlipperAngle * DEG_TO_RAD) - (oldAngleRight * DEG_TO_RAD)) / dt;
    }

    // Update Box2D bodies
    b2Rot leftRot = b2MakeRot(leftFlipperAngle * DEG_TO_RAD);
    b2Rot rightRot = b2MakeRot(rightFlipperAngle * DEG_TO_RAD);
    b2Body_SetTransform(*leftFlipperBody, b2Body_GetPosition(*leftFlipperBody), leftRot);
    b2Body_SetTransform(*rightFlipperBody, b2Body_GetPosition(*rightFlipperBody), rightRot);
    b2Body_SetAngularVelocity(*leftFlipperBody, deltaAngularVelocityLeft * flipperSpeedScalar);
    b2Body_SetAngularVelocity(*rightFlipperBody, deltaAngularVelocityRight * flipperSpeedScalar);

    // Return delta angular velocities for physics integration
    *out_leftDeltaAngularVelocity = deltaAngularVelocityLeft;
    *out_rightDeltaAngularVelocity = deltaAngularVelocityRight;
}
