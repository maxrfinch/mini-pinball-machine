/*
 * physics.c
 *
 * Responsibilities:
 *  - Owns all Chipmunk2D physics setup for the pinball table (space, walls, bumpers, flippers).
 *  - Registers Chipmunk collision handlers and applies their side effects (score, powerups, SFX).
 *  - Provides a small API used by the rest of the game via physics.h:
 *      - physics_init      : one-time setup for a GameStruct.
 *      - physics_step      : advance the simulation by dt.
 *      - physics_shutdown  : destroy the Chipmunk space for a GameStruct.
 *      - physics_add_ball  : spawn a new ball with initial position and velocity.
 *
 * Notes for future maintenance:
 *  - Keep Chipmunk-specific details (cpSpace, cpShape, cpBody, collision handlers) inside this file.
 *  - If you change the maximum number of balls or walls, update both this file and any matching
 *    configuration in constants.h / GameStruct so the array sizes stay in sync.
 *  - Prefer configuring tunable values (gravity, bumper positions, sizes, bounciness) via constants.h
 *    or a table at the top of this file rather than hardcoding them inline.
 */

#include "physics.h"
#include "constants.h"
#include "soundManager.h"
#include <stdio.h>
#include <math.h>


#define DEG_TO_RAD (3.14159265 / 180.0)

// Local geometry/limit constants for this module.
//
// numWalls   : number of static wall segments we create in physics_init().
// maxBalls   : must match the size of game->balls allocated/owned by GameStruct.
// ballSize   : default ball diameter (in world units) for type==0 balls.
//              If this changes, make sure any rendering code that assumes the
//              same size is updated as well.
// numBumpers : number of bumpers we create in physics_init().
static const int   numWalls   = 128;
static const int   maxBalls   = 256;
static const float ballSize   = 5.0f;
static const int   numBumpers = 14;

/*
 * BumperType:
 *  - Encodes gameplay semantics for bumpers so we avoid magic numbers.
 *  - Values must stay in sync with any rendering/UI code that branches on bumper->type.
 */
typedef enum {
    BUMPER_TYPE_STANDARD      = 0,  // Upper playfield round bumpers (score + powerup meter)
    BUMPER_TYPE_SLOW_MOTION   = 1,  // Special bumper that triggers slow motion + big score
    BUMPER_TYPE_LANE_TARGET_A = 2,  // Lane/target bumpers, group A
    BUMPER_TYPE_LANE_TARGET_B = 3,  // Lane/target bumpers, group B
    BUMPER_TYPE_WATER_POWERUP = 4   // Small bumpers that enable water powerup
} BumperType;

/* -------------------------------------------------------------------------- */
/*  Local animation state (driven by collision handlers, read by renderer)    */
/* -------------------------------------------------------------------------- */

static float leftLowerBumperAnim  = 0.0f;
static float rightLowerBumperAnim = 0.0f;

/*
 * CollisionHandlerLeftLowerBumper
 *  - Triggered when a ball hits the left lower slingshot/bumper segment.
 *  - Kicks the leftLowerBumperAnim value for rendering and awards points.
 */
static cpBool CollisionHandlerLeftLowerBumper(cpArbiter *arb, cpSpace *space, void *ignore){
	cpShape *a, *b;
	cpArbiterGetShapes(arb, &a, &b);

    if (!a || !b) {
        TraceLog(LOG_ERROR, "CollisionHandlerLeftLowerBumper: null shape pointer");
        return cpTrue;
    }

    Ball *ball = (Ball *)cpShapeGetUserData(a);
    if (!ball) {
        TraceLog(LOG_ERROR, "CollisionHandlerLeftLowerBumper: ball userData NULL");
        return cpTrue;
    }

    leftLowerBumperAnim = 1.0f;
    (ball->game)->gameScore += 25;
    if ((ball->game)->waterPowerupState == 0){
        (ball->game)->powerupScore += 25;
    }
    playBounce2((ball->game)->sound);
    return cpTrue;
}

/*
 * CollisionHandlerRightLowerBumper
 *  - Same as left lower bumper but for the right side.
 */
static cpBool CollisionHandlerRightLowerBumper(cpArbiter *arb, cpSpace *space, void *ignore){
	cpShape *a, *b;
	cpArbiterGetShapes(arb, &a, &b);

    if (!a || !b) {
        TraceLog(LOG_ERROR, "CollisionHandlerRightLowerBumper: null shape pointer");
        return cpTrue;
    }

    Ball *ball = (Ball *)cpShapeGetUserData(a);
    if (!ball) {
        TraceLog(LOG_ERROR, "CollisionHandlerRightLowerBumper: ball userData NULL");
        return cpTrue;
    }

    rightLowerBumperAnim = 1.0f;
    (ball->game)->gameScore += 25;
    if ((ball->game)->waterPowerupState == 0){
        (ball->game)->powerupScore += 25;
    }
    playBounce2((ball->game)->sound);
    return cpTrue;
}

/*
 * CollisionHandlerBallFlipper
 *  - Clears the ball's killCounter anytime it touches a flipper.
 *  - Prevents balls near the flippers from being treated as "stalled" and killed.
 */
static cpBool CollisionHandlerBallFlipper(cpArbiter *arb, cpSpace *space, void *ignore){
	cpShape *a, *b;
	cpArbiterGetShapes(arb, &a, &b);

    if (!a || !b) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallFlipper: null shape pointer");
        return cpTrue;
    }

    Ball *ball = (Ball *)cpShapeGetUserData(a);
    if (!ball) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallFlipper: ball userData NULL");
        return cpTrue;
    }

    ball->killCounter = 0;
    return cpTrue;
}

/*
 * CollisionHandlerBallBumper
 *  - Core gameplay collision for balls hitting the various bumper types.
 *  - Uses BumperType to determine scoring, powerups, sounds, and whether
 *    the collision should be processed by Chipmunk or ignored.
 */
static cpBool CollisionHandlerBallBumper(cpArbiter *arb, cpSpace *space, void *ignore){
	cpShape *a, *b;
	cpArbiterGetShapes(arb, &a, &b);

    if (!a || !b) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallBumper: null shape pointer");
        return cpTrue;
    }

	Bumper* bumper = (Bumper *)cpShapeGetUserData(b);
    Ball *ball = (Ball *)cpShapeGetUserData(a);

    if (!ball) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallBumper: ball userData NULL");
        return cpTrue;
    }
    if (!bumper) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallBumper: bumper userData NULL");
        return cpTrue;
    }

    if (bumper->index < 0 || bumper->index >= numBumpers) {
        TraceLog(LOG_ERROR, "CollisionHandlerBallBumper: invalid bumper index %d", bumper->index);
        return cpTrue;
    }

    if (bumper->type == BUMPER_TYPE_STANDARD){
        // Standard upper playfield bumper: visual bounce + modest score.
        bumper->bounceEffect = 10.0f;
        (ball->game)->gameScore += 50;
        if ((ball->game)->waterPowerupState == 0){
            (ball->game)->powerupScore += 50;
        }
        playUpperBouncerSound((ball->game)->sound);
        return cpTrue;
    } else if (bumper->type == BUMPER_TYPE_SLOW_MOTION){
        // Special bumper that triggers slow-motion mode and a large point bonus.
        (ball->game)->slowMotion = 1;
        (ball->game)->slowMotionCounter = 1200;
        (ball->game)->gameScore += 1000;
        if ((ball->game)->waterPowerupState == 0){
            (ball->game)->powerupScore += 1000;
        }
        playSlowdownSound((ball->game)->sound);
        bumper->bounceEffect = 20.0f;
    } else if (bumper->type == BUMPER_TYPE_LANE_TARGET_A ||
               bumper->type == BUMPER_TYPE_LANE_TARGET_B){
        // Lane/target bumpers: only award score once while enabled.
        if (bumper->enabled == 1){
            (ball->game)->gameScore += 50;
            if ((ball->game)->waterPowerupState == 0){
                (ball->game)->powerupScore += 50;
            }
            bumper->enabled = 0;
            playBounce((ball->game)->sound);
        }
    } else if (bumper->type == BUMPER_TYPE_WATER_POWERUP){
        // Small bumpers that drive the water powerup state.
        if (bumper->enabled == 1){
            bumper->bounceEffect = 10.0f;
            (ball->game)->gameScore += 250;
            if ((ball->game)->waterPowerupState == 0){
                (ball->game)->powerupScore += 250;
            }
            bumper->enabled = 0;
            playBounce((ball->game)->sound);
            return cpTrue;
        } else {
            return cpFalse;
        }
    } else {
        // Fallback: treat as a simple one-shot scoring bumper.
        (ball->game)->gameScore += 25;
        if ((ball->game)->waterPowerupState == 0){
            (ball->game)->powerupScore += 25;
        }
        bumper->enabled = 0;
    }

	return cpFalse;
}

/*
 * CollisionOneWay
 *  - Implements a one-way gate using the contact normal. If the ball is
 *    moving in the disallowed direction, we tell Chipmunk to ignore the
 *    contact so the ball passes through.
 */
static cpBool CollisionOneWay(cpArbiter *arb, cpSpace *space, void *ignore){
	cpShape *a, *b;
	cpArbiterGetShapes(arb, &a, &b);

    if (!a || !b) {
        TraceLog(LOG_ERROR, "CollisionOneWay: null shape pointer");
        return cpTrue;
    }

    printf("%f\n",cpvdot(cpArbiterGetNormal(arb), cpv(0,1)));
	if(cpvdot(cpArbiterGetNormal(arb), cpv(0,1)) < 0){
		return cpArbiterIgnore(arb);
	}

	return cpTrue;
}

/*
 * writeCircleWallSegment
 *  - Fills a contiguous range of entries in the walls[][] array with short line
 *    segments that approximate a circular arc.
 *  - segmentIndex: starting index in the walls array to write into.
 *  - numSegments : how many segments to generate (uses numSegments+1 points).
 *  - degStart/degEnd: arc range in degrees, before the internal -90° offset
 *    that aligns the arc with the table coordinate system.
 */
static void writeCircleWallSegment(float walls[numWalls][4], int segmentIndex, int numSegments,
                                   float degStart, float degEnd, float centerX, float centerY, float radius){
    float totalDegreeRange = fabsf(degEnd - degStart);
    float degPerPoint = totalDegreeRange / (numSegments + 1);

    degStart -= 90;

    // Iterate through numSegments + 1 points.
    float prevPointX = centerX + (cos(degStart * DEG_TO_RAD) * radius);
    float prevPointY = centerY + (sin(degStart * DEG_TO_RAD) * radius);
    float curPointX = 0;
    float curPointY = 0;
    for (int i = 1; i <= numSegments; i++){
        curPointX = centerX + (cos((degStart + (i * degPerPoint)) * DEG_TO_RAD) * radius);
        curPointY = centerY + (sin((degStart + (i * degPerPoint)) * DEG_TO_RAD) * radius);
        walls[segmentIndex + i - 1][0] = prevPointX;
        walls[segmentIndex + i - 1][1] = prevPointY;
        walls[segmentIndex + i - 1][2] = curPointX;
        walls[segmentIndex + i - 1][3] = curPointY;
        prevPointX = curPointX;
        prevPointY = curPointY;
    }
}

/*
 * physics_init
 *  - Creates a Chipmunk space for the given GameStruct.
 *  - Builds static walls, bumpers, one-way gate, and flippers.
 *  - Returns pointers to the bumper array and flipper bodies for use by
 *    rendering and game-logic code.
 */
void physics_init(GameStruct *game, Bumper **out_bumpers, cpBody **out_leftFlipperBody, cpBody **out_rightFlipperBody){
    /* ----------------------------- Static walls ----------------------------- */
    // Each entry is a segment: { x1, y1, x2, y2 } in world coordinates.
    float walls[numWalls][4] = {
        {0,0,worldWidth,0},
        {0,0,0,worldHeight},
        {worldWidth,0,worldWidth,worldHeight},
        {worldWidth - 6,56,worldWidth - 6,worldHeight},
        {worldWidth - 7,56,worldWidth - 7,worldHeight},
        {worldWidth-6,56,worldWidth-7,56},
        {0,128,19,142},
        {worldWidth - 7,128,worldWidth - 26,142},
        {0,2.1,worldWidth,2.1},
        {40.4,1.6,41.2,4.0},
        {41.2,4.0,65.2,1.6},
        {69.2,16.4,60.4,43.2},
        {60.4,43.2,68.8,55.6},
        {74.8,63.6,83.2,76.0},
        {84.0,56.7,84.0,37.2},
        {70.8,18.4,68,26.8},
        {74.8,37.6,68.8,55.6},
        {82.0,39.2,74.8,63.6},
        {67.400002,146.400009,83.200005,134.199997},
        {16.400000,146.199997,0.600000,134.600006}
    };
    writeCircleWallSegment(walls,20,20,0,90,worldWidth-28.5,30.75,28.75);
    writeCircleWallSegment(walls,40,20,270,360,28.5,30.75,28.75);
    writeCircleWallSegment(walls,60,10,20,110,64.75,35.6,10.15);
    writeCircleWallSegment(walls,70,10,20,110,64.75,35.6,17.50);
    writeCircleWallSegment(walls,80,10,13,110,64.75,35.6,19.50);

    // Initialize physics simulation
    cpVect gravity = cpv(0,100);
    cpSpace *space = cpSpaceNew();
    game->space = space;
    cpSpaceSetGravity(space,gravity);

    // Create walls
    for (int i = 0; i < numWalls; i++){
        cpShape *wall = cpSegmentShapeNew(cpSpaceGetStaticBody(space),cpv(walls[i][0],walls[i][1]),cpv(walls[i][2],walls[i][3]),0);
        cpShapeSetFriction(wall,0.5);
        cpShapeSetElasticity(wall,0.5);
        cpShapeSetCollisionType(wall, COLLISION_WALL);
        cpSpaceAddShape(space,wall);
    }

    /* ------------------------------ Bumpers --------------------------------- */
    // numBumpers and type assignments must stay in sync with rendering code.
    const float bumperSize = 10.0f;
    const float smallBumperSize = 4.0f;
    const float bumperBounciness = 1.8f;
    Bumper* bumpers = malloc(numBumpers * sizeof(Bumper));

    cpShape *bouncer1 = cpSegmentShapeNew(cpSpaceGetStaticBody(space),cpv(14.800000,125.200005),cpv(7.600000,109.200005),0);
    cpShape *bouncer2 = cpSegmentShapeNew(cpSpaceGetStaticBody(space),cpv(75.599998,108.800003),cpv(69.200005,125.200005),0);
    cpShapeSetCollisionType(bouncer1, COLLISION_LEFT_LOWER_BUMPER);
    cpShapeSetCollisionType(bouncer2, COLLISION_RIGHT_LOWER_BUMPER);

    cpShapeSetFriction(bouncer1,0.0);
    cpShapeSetElasticity(bouncer1,1.2f);
    cpSpaceAddShape(space,bouncer1);
    cpShapeSetFriction(bouncer2,0.0);
    cpShapeSetElasticity(bouncer2,1.2f);
    cpSpaceAddShape(space,bouncer2);
    cpShape *bouncerGuard1 = cpSegmentShapeNew(cpSpaceGetStaticBody(space),cpv(7.200000,111.200005),cpv(12.800000,124.400002),0);
    cpShape *bouncerGuard2 = cpSegmentShapeNew(cpSpaceGetStaticBody(space),cpv(71.200005,124.800003),cpv(76.000000,110.800003),0);
    cpShapeSetCollisionType(bouncerGuard1, COLLISION_WALL);
    cpShapeSetCollisionType(bouncerGuard2, COLLISION_WALL);
    cpSpaceAddShape(space,bouncerGuard1);
    cpShapeSetFriction(bouncerGuard1,0.0f);
    cpShapeSetElasticity(bouncerGuard1,0.9f);
    cpSpaceAddShape(space,bouncerGuard2);
    cpShapeSetFriction(bouncerGuard2,0.0f);
    cpShapeSetElasticity(bouncerGuard2,0.9f);

    bumpers[0].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[0].body,cpv(24.9,19.9));
    bumpers[0].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[0].body,bumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[0].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[0].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[0].shape,&bumpers[0]);
    bumpers[0].bounceEffect = 0;
    bumpers[0].type = BUMPER_TYPE_STANDARD;

    bumpers[1].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[1].body,cpv(46.6,17.8));
    bumpers[1].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[1].body,bumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[1].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[1].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[1].shape,&bumpers[1]);
    bumpers[1].bounceEffect = 0;
    bumpers[1].type = BUMPER_TYPE_STANDARD;

    bumpers[2].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[2].body,cpv(38.0,36.4));
    bumpers[2].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[2].body,bumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[2].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[2].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[2].shape,&bumpers[2]);
    bumpers[2].bounceEffect = 0;
    bumpers[2].type = BUMPER_TYPE_STANDARD;

    bumpers[3].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[3].body,cpv(72.200005,23.400000));
    bumpers[3].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[3].body,2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[3].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[3].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[3].shape,&bumpers[3]);
    bumpers[3].bounceEffect = 0;
    bumpers[3].type = BUMPER_TYPE_SLOW_MOTION;

    for (int i = 4; i < 10; i++){
        bumpers[i].body = cpSpaceAddBody(space,cpBodyNewKinematic());
        bumpers[i].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[i].body,2.0f,cpvzero));
        cpShapeSetElasticity(bumpers[i].shape,0);
        cpShapeSetCollisionType(bumpers[i].shape, COLLISION_BUMPER);
        cpShapeSetUserData(bumpers[i].shape,&bumpers[i]);
        bumpers[i].bounceEffect = 0;
        bumpers[i].type = BUMPER_TYPE_LANE_TARGET_A;
        bumpers[i].enabled = 1;
    }
    cpBodySetPosition(bumpers[4].body,cpv(63.34,50.88));
    cpBodySetPosition(bumpers[5].body,cpv(77.38,70.96));
    cpBodySetPosition(bumpers[6].body,cpv(15.1,62.04));
    cpBodySetPosition(bumpers[7].body,cpv(18.9,45.3));
    cpBodySetPosition(bumpers[8].body,cpv(61.02,35.36));
    cpBodySetPosition(bumpers[9].body,cpv(65.02,23.02));
    bumpers[4].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[5].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[6].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[7].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[8].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[9].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[4].angle = 90.0+145.2;
    bumpers[5].angle = 90.0+145.2;
    bumpers[6].angle = 90.0+25.7;
    bumpers[7].angle = 90;
    bumpers[8].angle = 90-162;
    bumpers[9].angle = 90-162;

    bumpers[10].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[10].body,cpv(12.2,81.8));
    bumpers[10].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[10].body,smallBumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[10].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[10].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[10].shape,&bumpers[10]);
    bumpers[10].bounceEffect = 0;
    bumpers[10].enabledSize = 0.0f;
    bumpers[10].enabled = 0;
    bumpers[10].type = BUMPER_TYPE_WATER_POWERUP;

    bumpers[11].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[11].body,cpv(23.8,91.2));
    bumpers[11].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[11].body,smallBumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[11].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[11].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[11].shape,&bumpers[11]);
    bumpers[11].bounceEffect = 0;
    bumpers[11].enabledSize = 0.0f;
    bumpers[11].enabled = 0;
    bumpers[11].type = BUMPER_TYPE_WATER_POWERUP;

    bumpers[12].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[12].body,cpv(61.2,91.2));
    bumpers[12].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[12].body,smallBumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[12].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[12].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[12].shape,&bumpers[12]);
    bumpers[12].bounceEffect = 0;
    bumpers[12].enabledSize = 0.0f;
    bumpers[12].enabled = 0;
    bumpers[12].type = BUMPER_TYPE_WATER_POWERUP;

    bumpers[13].body = cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(bumpers[13].body,cpv(72.599998,81.8));
    bumpers[13].shape = cpSpaceAddShape(space,cpCircleShapeNew(bumpers[13].body,smallBumperSize/2.0f,cpvzero));
    cpShapeSetElasticity(bumpers[13].shape,bumperBounciness);
    cpShapeSetCollisionType(bumpers[13].shape, COLLISION_BUMPER);
    cpShapeSetUserData(bumpers[13].shape,&bumpers[13]);
    bumpers[13].bounceEffect = 0;
    bumpers[13].enabledSize = 0.0f;
    bumpers[13].enabled = 0;
    bumpers[13].type = BUMPER_TYPE_WATER_POWERUP;

    //Add collision handler for ball-bumper effect
    cpCollisionHandler *handler = cpSpaceAddCollisionHandler(space,COLLISION_BALL,COLLISION_BUMPER);
    handler->beginFunc = CollisionHandlerBallBumper;

    cpCollisionHandler *ballFlipperHandler = cpSpaceAddCollisionHandler(space,COLLISION_BALL,COLLISION_PADDLE);
    ballFlipperHandler->preSolveFunc = CollisionHandlerBallFlipper;

    cpCollisionHandler *leftLower = cpSpaceAddCollisionHandler(space,COLLISION_BALL,COLLISION_LEFT_LOWER_BUMPER);
    cpCollisionHandler *rightLower = cpSpaceAddCollisionHandler(space,COLLISION_BALL,COLLISION_RIGHT_LOWER_BUMPER);
    leftLower->beginFunc = CollisionHandlerLeftLowerBumper;
    rightLower->beginFunc = CollisionHandlerRightLowerBumper;

    // create one-way door
	cpShape* oneWayShape = cpSpaceAddShape(space, cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(69.6,16.6), cpv(73.4,4.6), 0.5f));
	cpShapeSetElasticity(oneWayShape, 0.5f);
	cpShapeSetFriction(oneWayShape, 0.0f);
	cpShapeSetCollisionType(oneWayShape, COLLISION_ONE_WAY);
	cpCollisionHandler *oneWayHandler = cpSpaceAddCollisionHandler(space,COLLISION_BALL,COLLISION_ONE_WAY);
	oneWayHandler->preSolveFunc = CollisionOneWay;

	cpShape* tempShape = cpSpaceAddShape(space, cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(7.800000,38.200001), cpv(7.8,49.200001), 1.0f));
	cpShapeSetElasticity(tempShape, 0.5f);
	cpShapeSetFriction(tempShape, 0.5f);
	cpShapeSetCollisionType(tempShape, COLLISION_WALL);

	tempShape = cpSpaceAddShape(space, cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(16.000000,38.400002), cpv(16.000000,53.799999), 1.0f));
	cpShapeSetElasticity(tempShape, 0.5f);
	cpShapeSetFriction(tempShape, 0.5f);
	cpShapeSetCollisionType(tempShape, COLLISION_WALL);

	tempShape = cpSpaceAddShape(space, cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(16.000000,53.799999), cpv(8.600000,68.800003), 1.0f));
	cpShapeSetElasticity(tempShape, 0.5f);
	cpShapeSetFriction(tempShape, 0.5f);
	cpShapeSetCollisionType(tempShape, COLLISION_WALL);

    /* ------------------------------ Flippers -------------------------------- */
    // Create left and right flippers.
    cpBody* leftFlipperBody =  cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBody* rightFlipperBody =  cpSpaceAddBody(space,cpBodyNewKinematic());
    cpBodySetPosition(leftFlipperBody,cpv(19.8,145.45));
    cpBodySetPosition(rightFlipperBody,cpv(63.5,145.45));
    const cpVect flipperPoly[4] = {
        {0,0},
        {flipperWidth,0},
        {flipperWidth,flipperHeight},
        {0,flipperHeight}
    };
    cpShape* leftFlipperShape = cpSpaceAddShape(space,cpPolyShapeNewRaw(leftFlipperBody,4,flipperPoly,0.0f));
    cpShape* rightFlipperShape = cpSpaceAddShape(space,cpPolyShapeNewRaw(rightFlipperBody,4,flipperPoly,0.0f));
    cpShapeSetFriction(leftFlipperShape,0.8);
    cpShapeSetFriction(rightFlipperShape,0.8);
    cpShapeSetElasticity(leftFlipperShape,0.2);
    cpShapeSetElasticity(rightFlipperShape,0.2);
    cpShapeSetCollisionType(leftFlipperShape, COLLISION_PADDLE);
    cpShapeSetCollisionType(rightFlipperShape, COLLISION_PADDLE);
    cpBodySetCenterOfGravity(leftFlipperBody,cpv(flipperHeight/2.0f,flipperHeight/2.0f));
    cpBodySetCenterOfGravity(rightFlipperBody,cpv(flipperHeight/2.0f,flipperHeight/2.0f));

    // Return bumpers and flipper bodies to caller
    *out_bumpers = bumpers;
    *out_leftFlipperBody = leftFlipperBody;
    *out_rightFlipperBody = rightFlipperBody;
}

/*
 * physics_step
 *  - Advance the physics simulation by dt seconds.
 *  - All Chipmunk stepping should go through this function so we can
 *    centralize any future debug instrumentation or sub-stepping logic.
 */
void physics_step(GameStruct *game, float dt){
    TraceLog(LOG_INFO, "[PHYSICS] stepping dt=%f", dt);
    cpSpaceStep(game->space, dt);
    TraceLog(LOG_INFO, "[PHYSICS] done");
}

/*
 * physics_shutdown
 *  - Frees the Chipmunk space owned by this GameStruct.
 *  - Does not free GameStruct itself or any rendering data.
 */
void physics_shutdown(GameStruct *game){
    if (game->space != NULL){
        cpSpaceFree(game->space);
        game->space = NULL;
    }
}

/*
 * physics_add_ball
 *  - Spawns a new ball into the physics world, if there is capacity.
 *  - (px, py) : initial position in world units.
 *  - (vx, vy) : initial velocity.
 *  - type     : gameplay type (0 = normal, 2 = large, etc.).
 *
 *  Notes:
 *    - Updates game->numBalls and initializes the Ball struct at the chosen index.
 *    - Plays the launch sound via the SoundManager.
 */
void physics_add_ball(GameStruct *game, float px, float py, float vx, float vy, int type){
    if (game->numBalls < maxBalls){
        game->numBalls++;
        // Find the first index that isn't active
        int ballIndex = 0;
        for (int i = 0; i < maxBalls; i++){
            if (game->balls[i].active == 0){
                ballIndex = i;
                break;
            }
        }

        float radius = ballSize / 2.0;
        float mass = 1.0;
        if (type == 2){
            radius = 10.0f;
            mass = 2.0f;
        }
        cpFloat moment = cpMomentForCircle(mass, 0, radius, cpvzero);
        game->balls[ballIndex].body = cpSpaceAddBody(game->space,cpBodyNew(mass,moment));
        cpBodySetPosition(game->balls[ballIndex].body,cpv(px,py));
        cpBodySetVelocity(game->balls[ballIndex].body,cpv(vx,vy));
        game->balls[ballIndex].shape = cpSpaceAddShape(game->space,cpCircleShapeNew(game->balls[ballIndex].body,radius,cpvzero));
        cpShapeSetFriction(game->balls[ballIndex].shape,0.0);
        cpShapeSetElasticity(game->balls[ballIndex].shape,0.7);
        cpShapeSetCollisionType(game->balls[ballIndex].shape, COLLISION_BALL);
        cpShapeSetUserData(game->balls[ballIndex].shape,&(game->balls[ballIndex]));
        game->balls[ballIndex].active = 1;
        game->balls[ballIndex].game = game;
        game->balls[ballIndex].trailStartIndex = 0;
        game->balls[ballIndex].type = type;
        game->balls[ballIndex].killCounter = 0;
        if (type == 0){
            game->slowMotion = 0;
        }

        for (int i = 0; i< 16; i++){
            game->balls[ballIndex].locationHistoryX[i] = px;
            game->balls[ballIndex].locationHistoryY[i] = py;
        }

        playLaunch(game->sound);
    }
}
