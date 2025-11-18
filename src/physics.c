/*
 * physics.c - Box2D 3.x Physics Implementation (Migrated from Chipmunk)
 *
 * Responsibilities:
 *  - Owns all Box2D physics setup for the pinball table (world, walls, bumpers, flippers).
 *  - Registers Box2D collision handlers and applies their side effects (score, powerups, SFX).
 *  - Provides a small API used by the rest of the game via physics.h:
 *      - physics_init      : one-time setup for a GameStruct.
 *      - physics_step      : advance the simulation by dt.
 *      - physics_shutdown  : destroy the Box2D world for a GameStruct.
 *      - physics_add_ball  : spawn a new ball with initial position and velocity.
 *
 * Collision Category Mapping (Chipmunk → Box2D):
 * ===============================================
 * This implementation is a 1:1 port from physics_old.c (Chipmunk).
 * 
 * Chipmunk used integer collision types:
 *   - COLLISION_WALL              = 0
 *   - COLLISION_BALL              = 1
 *   - COLLISION_BUMPER            = 2
 *   - COLLISION_PADDLE            = 3
 *   - COLLISION_LEFT_LOWER_BUMPER = 4
 *   - COLLISION_RIGHT_LOWER_BUMPER= 5
 *   - COLLISION_ONE_WAY           = 6
 * 
 * Box2D uses bit flags for filtering:
 *   - CATEGORY_WALL                = (1 << 0) = 0x0001
 *   - CATEGORY_BALL                = (1 << 1) = 0x0002
 *   - CATEGORY_BUMPER              = (1 << 2) = 0x0004
 *   - CATEGORY_PADDLE              = (1 << 3) = 0x0008
 *   - CATEGORY_LEFT_LOWER_BUMPER   = (1 << 4) = 0x0010
 *   - CATEGORY_RIGHT_LOWER_BUMPER  = (1 << 5) = 0x0020
 *   - CATEGORY_ONE_WAY             = (1 << 6) = 0x0040
 * 
 * All collision logic from physics_old.c is implemented in PreSolveCallback().
 * See the detailed comment above PreSolveCallback for handler-by-handler mapping.
 *
 * Notes for future maintenance:
 *  - Keep Box2D-specific details (b2WorldId, b2ShapeId, b2BodyId, collision callbacks) inside this file.
 *  - If you change the maximum number of balls or walls, update both this file and any matching
 *    configuration in constants.h / GameStruct so the array sizes stay in sync.
 *  - Prefer configuring tunable values (gravity, bumper positions, sizes, bounciness) via constants.h
 *    or a table at the top of this file rather than hardcoding them inline.
 *  - Contact return values: return true to allow collision, return false to disable (make "ghost").
 */

#include "physics.h"
#include "constants.h"
#include "soundManager.h"
#include "physicsDebugDraw.h"
#include <stdio.h>
#include <math.h>
#include <box2d/box2d.h>

#define DEG_TO_RAD (3.14159265 / 180.0)

// External function from main.c for water ripple impacts
extern void AddWaterImpulse(float x, float impulse);

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

// These variables are accessed by main.c for rendering lower bumper animations
// They are set to 1.0f when a collision occurs and decremented in main.c
float leftLowerBumperAnim  = 0.0f;
float rightLowerBumperAnim = 0.0f;

/* -------------------------------------------------------------------------- */
/*  Debug draw state - stores references to bodies and shapes for rendering  */
/* -------------------------------------------------------------------------- */

typedef struct {
    b2BodyId staticBody;      // The main static body holding all walls
    b2BodyId *leftFlipper;    // Left flipper body pointer
    b2BodyId *rightFlipper;   // Right flipper body pointer
    Bumper *bumpers;          // Pointer to bumpers array
    int numBumpers;           // Number of bumpers
} PhysicsDebugState;

static PhysicsDebugState debugState = {0};

/* -------------------------------------------------------------------------- */
/*  Helper function to create a b2Vec2                                        */
/* -------------------------------------------------------------------------- */
static inline b2Vec2 pb2_v(float x, float y) {
    b2Vec2 v = {x, y};
    return v;
}

/* -------------------------------------------------------------------------- */
/*  Box2D 3.x Contact Event Handlers                                          */
/* -------------------------------------------------------------------------- */

/*
 * COLLISION HANDLER MAPPING: Chipmunk (physics_old.c) → Box2D (physics.c)
 * =========================================================================
 * 
 * This PreSolveCallback implements all collision logic from physics_old.c.
 * Each Chipmunk collision handler is mapped to a branch in this function.
 * 
 * Category Bits → Gameplay Entities:
 * ----------------------------------
 * CATEGORY_WALL                (0x0001) : Static walls, boundaries
 * CATEGORY_BALL                (0x0002) : Player balls
 * CATEGORY_BUMPER              (0x0004) : All bumpers (type determined by userData)
 * CATEGORY_PADDLE              (0x0008) : Flippers
 * CATEGORY_LEFT_LOWER_BUMPER   (0x0010) : Left slingshot
 * CATEGORY_RIGHT_LOWER_BUMPER  (0x0020) : Right slingshot
 * CATEGORY_ONE_WAY             (0x0040) : Shooter lane gate
 * 
 * Chipmunk Handler → Box2D Branch Mapping:
 * ----------------------------------------
 * CollisionHandlerBallBumper (BEGIN)
 *   → if (otherCategory == CATEGORY_BUMPER)
 *   → Returns cpTrue for elastic bounce or cpFalse to disable contact
 *   → Box2D: return true for elastic, return false to disable
 * 
 * CollisionHandlerBallFlipper (PRESOLVE)
 *   → if (otherCategory == CATEGORY_PADDLE)
 *   → Returns cpTrue, clears ball->killCounter
 *   → Box2D: return true, clears ball->killCounter
 * 
 * CollisionHandlerLeftLowerBumper (BEGIN)
 *   → if (otherCategory == CATEGORY_LEFT_LOWER_BUMPER)
 *   → Returns cpTrue, sets leftLowerBumperAnim, awards 25 points
 *   → Box2D: return true, same side effects
 * 
 * CollisionHandlerRightLowerBumper (BEGIN)
 *   → if (otherCategory == CATEGORY_RIGHT_LOWER_BUMPER)
 *   → Returns cpTrue, sets rightLowerBumperAnim, awards 25 points
 *   → Box2D: return true, same side effects
 * 
 * CollisionOneWay (PRESOLVE)
 *   → if (otherCategory == CATEGORY_ONE_WAY)
 *   → Uses cpvdot(cpArbiterGetNormal(arb), cpv(0,1)) to check direction
 *   → Returns cpArbiterIgnore(arb) if dot < 0 (pass through), cpTrue if >= 0 (block)
 *   → Box2D: return false if normal dot (0,1) < 0, return true otherwise
 *   → CRITICAL: Must account for contact normal direction (depends on shape order)
 */

/*
 * Box2D 3.x uses a custom PreSolve callback function.
 * This function is called for each contact before the solver.
 * We use it to implement collision logic and optionally disable contacts.
 */
static bool PreSolveCallback(b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Manifold* manifold, void* context) {
    // Get the collision category bits to identify what's colliding
    b2Filter filterA = b2Shape_GetFilter(shapeIdA);
    b2Filter filterB = b2Shape_GetFilter(shapeIdB);
    
    uint32_t catA = filterA.categoryBits;
    uint32_t catB = filterB.categoryBits;
    
    // Identify the ball and the other object
    b2ShapeId ballShapeId = b2_nullShapeId;
    b2ShapeId otherShapeId = b2_nullShapeId;
    uint32_t otherCategory = 0;
    
    if (catA == CATEGORY_BALL) {
        ballShapeId = shapeIdA;
        otherShapeId = shapeIdB;
        otherCategory = catB;
    } else if (catB == CATEGORY_BALL) {
        ballShapeId = shapeIdB;
        otherShapeId = shapeIdA;
        otherCategory = catA;
    } else {
        // Neither is a ball, allow collision
        return true;
    }
    
    // Get ball user data
    Ball *ball = (Ball *)b2Shape_GetUserData(ballShapeId);
    if (!ball) {
        return true;
    }
    
    // Handle different collision types
    if (otherCategory == CATEGORY_BUMPER) {
        // Ball-Bumper collision
        Bumper *bumper = (Bumper *)b2Shape_GetUserData(otherShapeId);
        if (!bumper) {
            return true;
        }
        
        if (bumper->type == BUMPER_TYPE_STANDARD) {
            // Standard bumpers: apply bounce effect, score, sound, and allow elastic collision
            bumper->bounceEffect = 10.0f;
            (ball->game)->gameScore += 50;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 50;
            }
            playUpperBouncerSound((ball->game)->sound);
            return true;
        } else if (bumper->type == BUMPER_TYPE_SLOW_MOTION) {
            // Slow-motion bumper: trigger effect but disable elastic collision
            // In Chipmunk this returned cpFalse to prevent elastic bounce
            (ball->game)->slowMotion = 1;
            (ball->game)->slowMotionCounter = 1200;
            (ball->game)->gameScore += 1000;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 1000;
            }
            playSlowdownSound((ball->game)->sound);
            bumper->bounceEffect = 20.0f;
            return false; // Disable contact to prevent elastic collision
        } else if (bumper->type == BUMPER_TYPE_LANE_TARGET_A ||
                   bumper->type == BUMPER_TYPE_LANE_TARGET_B) {
            // Lane target bumpers: score once when enabled, then disable collision
            // In Chipmunk this returned cpFalse to prevent elastic bounce
            if (bumper->enabled == 1) {
                (ball->game)->gameScore += 50;
                if ((ball->game)->waterPowerupState == 0) {
                    (ball->game)->powerupScore += 50;
                }
                bumper->enabled = 0;
                playBounce((ball->game)->sound);
            }
            return false; // Disable contact - lane targets don't bounce
        } else if (bumper->type == BUMPER_TYPE_WATER_POWERUP) {
            if (bumper->enabled == 1) {
                bumper->bounceEffect = 10.0f;
                (ball->game)->gameScore += 250;
                if ((ball->game)->waterPowerupState == 0) {
                    (ball->game)->powerupScore += 250;
                }
                bumper->enabled = 0;
                playBounce((ball->game)->sound);
                return true; // Allow elastic collision for enabled water powerup bumpers
            } else {
                return false; // Disable collision if bumper not enabled
            }
        } else {
            // Fallback: simple one-shot scoring bumper without elastic bounce
            // In Chipmunk this returned cpFalse to prevent elastic bounce
            (ball->game)->gameScore += 25;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 25;
            }
            bumper->enabled = 0;
            return false; // Disable contact for fallback bumpers
        }
    } else if (otherCategory == CATEGORY_PADDLE) {
        // Ball-Flipper collision
        ball->killCounter = 0;
        return true;
    } else if (otherCategory == CATEGORY_LEFT_LOWER_BUMPER) {
        // Left lower slingshot
        leftLowerBumperAnim = 1.0f;
        (ball->game)->gameScore += 25;
        if ((ball->game)->waterPowerupState == 0) {
            (ball->game)->powerupScore += 25;
        }
        playBounce2((ball->game)->sound);
        return true;
    } else if (otherCategory == CATEGORY_RIGHT_LOWER_BUMPER) {
        // Right lower slingshot
        rightLowerBumperAnim = 1.0f;
        (ball->game)->gameScore += 25;
        if ((ball->game)->waterPowerupState == 0) {
            (ball->game)->powerupScore += 25;
        }
        playBounce2((ball->game)->sound);
        return true;
    } else if (otherCategory == CATEGORY_ONE_WAY) {
        // One-way gate logic - ported from Chipmunk's CollisionOneWay handler
        // 
        // Chipmunk version:
        //   if (cpvdot(cpArbiterGetNormal(arb), cpv(0,1)) < 0) {
        //       return cpArbiterIgnore(arb);  // Pass through
        //   }
        //   return cpTrue;  // Block
        //
        // The gate segment is at (69.6, 16.6) to (73.4, 4.6) in the shooter lane.
        // When the ball passes upward (leaving the shooter), the dot product with (0,1) is negative.
        // When the ball tries to fall back down, the dot product is positive or zero.
        //
        // Box2D manifold normal: points from shapeA to shapeB
        // We need to determine which direction the normal is pointing relative to upward (0,1).
        // 
        // The manifold normal in Box2D points from shape A to shape B.
        // Since we identified the ball as either A or B, we need to know which.
        b2Vec2 normal = manifold->normal;
        
        // If ball is shape A, normal points from ball to gate.
        // If ball is shape B, normal points from gate to ball (so we need to flip it).
        if (catB == CATEGORY_BALL) {
            // Ball is shape B, so normal points from gate (A) to ball (B)
            // We need normal from ball to gate, so flip it
            normal.x = -normal.x;
            normal.y = -normal.y;
        }
        
        // Compute dot product with upward direction (0, 1)
        float dotProduct = normal.x * 0.0f + normal.y * 1.0f;
        
        if (dotProduct < 0) {
            // Ball is moving upward through the gate (leaving shooter lane)
            return false; // Disable contact - allow pass through
        }
        
        // Ball is trying to fall back down into shooter lane
        return true; // Block the ball
    }
    
    return true; // Allow collision by default
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
                                   float degStart, float degEnd, float centerX, float centerY, float radius) {
    float totalDegreeRange = fabsf(degEnd - degStart);
    float degPerPoint = totalDegreeRange / (numSegments + 1);

    degStart -= 90;

    // Iterate through numSegments + 1 points.
    float prevPointX = centerX + (cos(degStart * DEG_TO_RAD) * radius);
    float prevPointY = centerY + (sin(degStart * DEG_TO_RAD) * radius);
    float curPointX = 0;
    float curPointY = 0;
    for (int i = 1; i <= numSegments; i++) {
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
 *  - Creates a Box2D world for the given GameStruct.
 *  - Builds static walls, bumpers, one-way gate, and flippers.
 *  - Returns pointers to the bumper array and flipper bodies for use by
 *    rendering and game-logic code.
 */
void physics_init(GameStruct *game, Bumper **out_bumpers, b2BodyId **out_leftFlipperBody, b2BodyId **out_rightFlipperBody) {
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
    writeCircleWallSegment(walls, 20, 20, 0, 90, worldWidth-28.5, 30.75, 28.75);
    writeCircleWallSegment(walls, 40, 20, 270, 360, 28.5, 30.75, 28.75);
    writeCircleWallSegment(walls, 60, 10, 20, 110, 64.75, 35.6, 10.15);
    writeCircleWallSegment(walls, 70, 10, 20, 110, 64.75, 35.6, 17.50);
    writeCircleWallSegment(walls, 80, 10, 13, 110, 64.75, 35.6, 19.50);

    // Initialize physics simulation
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = pb2_v(0, 100);

    game->world = b2CreateWorld(&worldDef);
    
    // Register the PreSolve callback for collision handling
    b2World_SetPreSolveCallback(game->world, PreSolveCallback, NULL);

    // Create static body for walls
    b2BodyDef staticBodyDef = b2DefaultBodyDef();
    staticBodyDef.type = b2_staticBody;
    staticBodyDef.position = pb2_v(0, 0);
    b2BodyId staticBody = b2CreateBody(game->world, &staticBodyDef);

    // Create walls
    for (int i = 0; i < numWalls; i++) {
        b2Segment segment;
        segment.point1 = pb2_v(walls[i][0], walls[i][1]);
        segment.point2 = pb2_v(walls[i][2], walls[i][3]);

        b2ShapeDef shapeDef = b2DefaultShapeDef();
        shapeDef.material.friction = 0.5f;
        shapeDef.material.restitution = 0.5f;
        shapeDef.filter.categoryBits = CATEGORY_WALL;
        shapeDef.filter.maskBits     = CATEGORY_BALL;

        b2CreateSegmentShape(staticBody, &shapeDef, &segment);
    }

    /* ------------------------------ Bumpers --------------------------------- */
    // numBumpers and type assignments must stay in sync with rendering code.
    const float bumperSize = 10.0f;
    const float smallBumperSize = 4.0f;
    const float bumperBounciness = 1.8f;
    Bumper* bumpers = malloc(numBumpers * sizeof(Bumper));

    // Lower slingshot segments
    b2Segment leftBouncer, rightBouncer;
    leftBouncer.point1 = pb2_v(14.800000, 125.200005);
    leftBouncer.point2 = pb2_v(7.600000, 109.200005);
    rightBouncer.point1 = pb2_v(75.599998, 108.800003);
    rightBouncer.point2 = pb2_v(69.200005, 125.200005);
    
    b2ShapeDef leftBouncerDef = b2DefaultShapeDef();
    leftBouncerDef.material.friction = 0.0f;
    leftBouncerDef.material.restitution = 1.2f;
    leftBouncerDef.filter.categoryBits = CATEGORY_LEFT_LOWER_BUMPER;
    leftBouncerDef.filter.maskBits     = CATEGORY_BALL;
    b2CreateSegmentShape(staticBody, &leftBouncerDef, &leftBouncer);

    b2ShapeDef rightBouncerDef = b2DefaultShapeDef();
    rightBouncerDef.material.friction = 0.0f;
    rightBouncerDef.material.restitution = 1.2f;
    rightBouncerDef.filter.categoryBits = CATEGORY_RIGHT_LOWER_BUMPER;
    rightBouncerDef.filter.maskBits     = CATEGORY_BALL;
    b2CreateSegmentShape(staticBody, &rightBouncerDef, &rightBouncer);

    // Bouncer guards
    b2Segment guard1, guard2;
    guard1.point1 = pb2_v(7.200000, 111.200005);
    guard1.point2 = pb2_v(12.800000, 124.400002);
    guard2.point1 = pb2_v(71.200005, 124.800003);
    guard2.point2 = pb2_v(76.000000, 110.800003);
    
    b2ShapeDef guardDef = b2DefaultShapeDef();
    guardDef.material.friction = 0.0f;
    guardDef.material.restitution = 0.9f;
    guardDef.filter.categoryBits = CATEGORY_WALL;
    guardDef.filter.maskBits     = CATEGORY_BALL;
    b2CreateSegmentShape(staticBody, &guardDef, &guard1);
    b2CreateSegmentShape(staticBody, &guardDef, &guard2);

    // Standard bumpers (0-2)
    float bumperPositions[3][2] = {
        {24.9, 19.9},
        {46.6, 17.8},
        {38.0, 36.4}
    };

    for (int i = 0; i < 3; i++) {
        b2BodyDef bumperBodyDef = b2DefaultBodyDef();
        bumperBodyDef.type = b2_kinematicBody;
        bumperBodyDef.position = pb2_v(bumperPositions[i][0], bumperPositions[i][1]);
        bumpers[i].body = b2CreateBody(game->world, &bumperBodyDef);

        b2Circle circle;
        circle.center = pb2_v(0, 0);
        circle.radius = bumperSize / 2.0f;

        b2ShapeDef bumperShapeDef = b2DefaultShapeDef();
        bumperShapeDef.material.restitution = bumperBounciness;
        bumperShapeDef.filter.categoryBits = CATEGORY_BUMPER;
        bumperShapeDef.filter.maskBits     = CATEGORY_BALL;
        bumperShapeDef.userData = &bumpers[i];

        bumpers[i].shape = b2CreateCircleShape(bumpers[i].body, &bumperShapeDef, &circle);
        bumpers[i].bounceEffect = 0;
        bumpers[i].type = BUMPER_TYPE_STANDARD;
    }

    // Slow motion bumper (3)
    b2BodyDef slowMoBumperDef = b2DefaultBodyDef();
    slowMoBumperDef.type = b2_kinematicBody;
    slowMoBumperDef.position = pb2_v(72.200005, 23.400000);
    bumpers[3].body = b2CreateBody(game->world, &slowMoBumperDef);

    b2Circle slowMoCircle;
    slowMoCircle.center = pb2_v(0, 0);
    slowMoCircle.radius = 2.0f;

    b2ShapeDef slowMoShapeDef = b2DefaultShapeDef();
    slowMoShapeDef.material.restitution = bumperBounciness;
    slowMoShapeDef.filter.categoryBits = CATEGORY_BUMPER;
    slowMoShapeDef.filter.maskBits     = CATEGORY_BALL;
    slowMoShapeDef.userData = &bumpers[3];

    bumpers[3].shape = b2CreateCircleShape(bumpers[3].body, &slowMoShapeDef, &slowMoCircle);
    bumpers[3].bounceEffect = 0;
    bumpers[3].type = BUMPER_TYPE_SLOW_MOTION;

    // Lane target bumpers (4-9)
    float lanePositions[6][2] = {
        {63.34, 50.88},
        {77.38, 70.96},
        {15.1, 62.04},
        {18.9, 45.3},
        {61.02, 35.36},
        {65.02, 23.02}
    };
    BumperType laneTypes[6] = {
        BUMPER_TYPE_LANE_TARGET_A,
        BUMPER_TYPE_LANE_TARGET_A,
        BUMPER_TYPE_LANE_TARGET_A,
        BUMPER_TYPE_LANE_TARGET_B,
        BUMPER_TYPE_LANE_TARGET_B,
        BUMPER_TYPE_LANE_TARGET_B
    };
    float laneAngles[6] = {
        90.0 + 145.2,
        90.0 + 145.2,
        90.0 + 25.7,
        90.0,
        90.0 - 162.0,
        90.0 - 162.0
    };

    for (int i = 4; i < 10; i++) {
        b2BodyDef laneBumperDef = b2DefaultBodyDef();
        laneBumperDef.type = b2_kinematicBody;
        laneBumperDef.position = pb2_v(lanePositions[i-4][0], lanePositions[i-4][1]);
        bumpers[i].body = b2CreateBody(game->world, &laneBumperDef);

        b2Circle laneCircle;
        laneCircle.center = pb2_v(0, 0);
        laneCircle.radius = 2.0f;

        b2ShapeDef laneShapeDef = b2DefaultShapeDef();
        laneShapeDef.material.restitution = 0.0f;
        laneShapeDef.filter.categoryBits = CATEGORY_BUMPER;
        laneShapeDef.filter.maskBits     = CATEGORY_BALL;
        laneShapeDef.userData = &bumpers[i];

        bumpers[i].shape = b2CreateCircleShape(bumpers[i].body, &laneShapeDef, &laneCircle);
        bumpers[i].bounceEffect = 0;
        bumpers[i].type = laneTypes[i-4];
        bumpers[i].enabled = 1;
        bumpers[i].angle = laneAngles[i-4];
    }

    // Water powerup bumpers (10-13)
    float waterPositions[4][2] = {
        {12.2, 81.8},
        {23.8, 91.2},
        {61.2, 91.2},
        {72.599998, 81.8}
    };

    for (int i = 10; i < 14; i++) {
        b2BodyDef waterBumperDef = b2DefaultBodyDef();
        waterBumperDef.type = b2_kinematicBody;
        waterBumperDef.position = pb2_v(waterPositions[i-10][0], waterPositions[i-10][1]);
        bumpers[i].body = b2CreateBody(game->world, &waterBumperDef);

        b2Circle waterCircle;
        waterCircle.center = pb2_v(0, 0);
        waterCircle.radius = smallBumperSize / 2.0f;

        b2ShapeDef waterShapeDef = b2DefaultShapeDef();
        waterShapeDef.material.restitution = bumperBounciness;
        waterShapeDef.filter.categoryBits = CATEGORY_BUMPER;
        waterShapeDef.filter.maskBits     = CATEGORY_BALL;
        waterShapeDef.userData = &bumpers[i];

        bumpers[i].shape = b2CreateCircleShape(bumpers[i].body, &waterShapeDef, &waterCircle);
        bumpers[i].bounceEffect = 0;
        bumpers[i].enabledSize = 0.0f;
        bumpers[i].enabled = 0;
        bumpers[i].type = BUMPER_TYPE_WATER_POWERUP;
    }

    // Create one-way door
    b2Segment oneWaySegment;
    oneWaySegment.point1 = pb2_v(69.6, 16.6);
    oneWaySegment.point2 = pb2_v(73.4, 4.6);
    
    b2ShapeDef oneWayDef = b2DefaultShapeDef();
    oneWayDef.material.restitution = 0.5f;
    oneWayDef.material.friction = 0.0f;
    oneWayDef.filter.categoryBits = CATEGORY_ONE_WAY;
    oneWayDef.filter.maskBits     = CATEGORY_BALL;
    b2CreateSegmentShape(staticBody, &oneWayDef, &oneWaySegment);

    // Additional static segments
    b2Segment tempSegments[3];
    tempSegments[0].point1 = pb2_v(7.800000, 38.200001);
    tempSegments[0].point2 = pb2_v(7.8, 49.200001);
    tempSegments[1].point1 = pb2_v(16.000000, 38.400002);
    tempSegments[1].point2 = pb2_v(16.000000, 53.799999);
    tempSegments[2].point1 = pb2_v(16.000000, 53.799999);
    tempSegments[2].point2 = pb2_v(8.600000, 68.800003);

    b2ShapeDef tempDef = b2DefaultShapeDef();
    tempDef.material.restitution = 0.5f;
    tempDef.material.friction = 0.5f;
    tempDef.filter.categoryBits = CATEGORY_WALL;
    tempDef.filter.maskBits     = CATEGORY_BALL;

    for (int i = 0; i < 3; i++) {
        b2CreateSegmentShape(staticBody, &tempDef, &tempSegments[i]);
    }

    /* ------------------------------ Flippers -------------------------------- */
    // Allocate memory for flipper bodies to return
    static b2BodyId leftFlipperBodyStatic;
    static b2BodyId rightFlipperBodyStatic;

    // Create left flipper
    // Body position must be adjusted because we offset the polygon vertices
    // Original Chipmunk position: (19.8, 145.45) with polygon at (0,0) and CoG at (height/2, height/2)
    // Box2D: We offset polygon by (-height/2, -height/2), so adjust body position by +(height/2, height/2)
    b2BodyDef leftFlipperDef = b2DefaultBodyDef();
    leftFlipperDef.type = b2_kinematicBody;
    leftFlipperDef.position = pb2_v(17.2 + flipperHeight / 2.0f, 142.8 + flipperHeight / 2.0f);
    leftFlipperBodyStatic = b2CreateBody(game->world, &leftFlipperDef);

    // Create right flipper
    b2BodyDef rightFlipperDef = b2DefaultBodyDef();
    rightFlipperDef.type = b2_kinematicBody;
    rightFlipperDef.position = pb2_v(61.4 + flipperHeight / 2.0f, 142.8 + flipperHeight / 2.0f);
    rightFlipperBodyStatic = b2CreateBody(game->world, &rightFlipperDef);

    // Define flipper polygon shape
    // In Chipmunk, the polygon was at (0,0) to (width, height) with center of gravity at (height/2, height/2)
    // In Box2D, we achieve the same effect by offsetting the polygon vertices so the rotation point is at (0,0)
    // Original vertices: (0,0), (width,0), (width,height), (0,height)
    // Offset by (-height/2, -height/2) to put rotation point at origin:
    float pivotOffsetX = -flipperHeight / 2.0f;
    float pivotOffsetY = -flipperHeight / 2.0f;
    
    b2Vec2 flipperVerts[4] = {
        pb2_v(pivotOffsetX, pivotOffsetY),                              // Bottom-left
        pb2_v(flipperWidth + pivotOffsetX, pivotOffsetY),               // Bottom-right
        pb2_v(flipperWidth + pivotOffsetX, flipperHeight + pivotOffsetY), // Top-right
        pb2_v(pivotOffsetX, flipperHeight + pivotOffsetY)               // Top-left
    };

    b2Hull flipperHull = b2ComputeHull(flipperVerts, 4);
    b2Polygon flipperPoly = b2MakePolygon(&flipperHull, 0);

    // Create left flipper shape
    b2ShapeDef leftFlipperShapeDef = b2DefaultShapeDef();
    leftFlipperShapeDef.material.friction = 0.8f;
    leftFlipperShapeDef.material.restitution = 0.2f;
    leftFlipperShapeDef.filter.categoryBits = CATEGORY_PADDLE;
    leftFlipperShapeDef.filter.maskBits     = CATEGORY_BALL;
    b2CreatePolygonShape(leftFlipperBodyStatic, &leftFlipperShapeDef, &flipperPoly);

    // Create right flipper shape
    b2ShapeDef rightFlipperShapeDef = b2DefaultShapeDef();
    rightFlipperShapeDef.material.friction = 0.8f;
    rightFlipperShapeDef.material.restitution = 0.2f;
    rightFlipperShapeDef.filter.categoryBits = CATEGORY_PADDLE;
    rightFlipperShapeDef.filter.maskBits     = CATEGORY_BALL;
    b2CreatePolygonShape(rightFlipperBodyStatic, &rightFlipperShapeDef, &flipperPoly);

    // Store references for debug drawing
    debugState.staticBody = staticBody;
    debugState.leftFlipper = &leftFlipperBodyStatic;
    debugState.rightFlipper = &rightFlipperBodyStatic;
    debugState.bumpers = bumpers;
    debugState.numBumpers = numBumpers;

    // Return bumpers and flipper bodies to caller
    *out_bumpers = bumpers;
    *out_leftFlipperBody = &leftFlipperBodyStatic;
    *out_rightFlipperBody = &rightFlipperBodyStatic;
}

/*
 * physics_step
 *  - Advance the physics simulation by dt seconds.
 *  - All Box2D stepping should go through this function so we can
 *    centralize any future debug instrumentation or sub-stepping logic.
 */
void physics_step(GameStruct *game, float dt) {
    //TraceLog(LOG_INFO, "[PHYSICS] stepping dt=%f", dt);
    
    // Box2D 3.x step parameters
    // Use subStepCount=1 to match Chipmunk's single-step behavior
    // Higher substep counts can cause excessive velocity accumulation
    int subStepCount = 1;
    b2World_Step(game->world, dt, subStepCount);
    
    // Contact events are processed during PreSolveCallback
    // which is called automatically during b2World_Step
    
    // Check for water intersections and apply ripple impulses
    if (game->waterPowerupState > 0) {
        // Water level in world coordinates (water rises from bottom)
        float waterWorldY = worldHeight * (1.0f - game->waterHeight);
        
        for (int i = 0; i < maxBalls; i++) {
            if (game->balls[i].active) {
                b2Vec2 pos = b2Body_GetPosition(game->balls[i].body);
                b2Vec2 vel = b2Body_GetLinearVelocity(game->balls[i].body);
                
                // Check if ball just entered water (was above, now at or below water level)
                int wasUnderwater = game->balls[i].underwaterState;
                int isUnderwater = (pos.y >= waterWorldY) ? 1 : 0;
                
                // On water entry, create ripple impulse
                if (!wasUnderwater && isUnderwater) {
                    // Map ball x-position (0 to worldWidth) to ripple index
                    float impulse = fabsf(vel.y) * 0.0025f;
                    AddWaterImpulse(pos.x, impulse);
                }
                
                game->balls[i].underwaterState = isUnderwater;
            }
        }
    }
    
    //TraceLog(LOG_INFO, "[PHYSICS] done");
}

/*
 * physics_shutdown
 *  - Frees the Box2D world owned by this GameStruct.
 *  - Does not free GameStruct itself or any rendering data.
 */
void physics_shutdown(GameStruct *game) {
    if (B2_IS_NON_NULL(game->world)) {
        b2DestroyWorld(game->world);
        game->world = b2_nullWorldId;
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
void physics_add_ball(GameStruct *game, float px, float py, float vx, float vy, int type) {
    if (game->numBalls < maxBalls) {
        game->numBalls++;
        // Find the first index that isn't active
        int ballIndex = 0;
        for (int i = 0; i < maxBalls; i++) {
            if (game->balls[i].active == 0) {
                ballIndex = i;
                break;
            }
        }

        float radius = ballSize / 2.0;
        float mass = 1.0;
        if (type == 2) {
            radius = 10.0f;
            mass = 2.0f;
        }
        
        // Calculate density from mass to match Chipmunk behavior
        // In Chipmunk: mass is set directly
        // In Box2D: mass = density * area, where area = π * r²
        // Therefore: density = mass / (π * r²)
        float area = 3.14159265f * radius * radius;
        float density = mass / area;

        // Create ball body
        b2BodyDef ballBodyDef = b2DefaultBodyDef();
        ballBodyDef.type = b2_dynamicBody;
        ballBodyDef.position = pb2_v(px, py);
        ballBodyDef.linearVelocity = pb2_v(vx, vy);
        game->balls[ballIndex].body = b2CreateBody(game->world, &ballBodyDef);

        // Create ball shape
        b2Circle ballCircle;
        ballCircle.center = pb2_v(0, 0);
        ballCircle.radius = radius;

        b2ShapeDef ballShapeDef = b2DefaultShapeDef();
        ballShapeDef.enableContactEvents = true;
        ballShapeDef.enablePreSolveEvents = true;
        ballShapeDef.material.friction = 0.0f;
        ballShapeDef.material.restitution = 0.7f;
        ballShapeDef.density = density;
        ballShapeDef.filter.categoryBits = CATEGORY_BALL;
        ballShapeDef.filter.maskBits =
            CATEGORY_WALL |
            CATEGORY_BUMPER |
            CATEGORY_PADDLE |
            CATEGORY_LEFT_LOWER_BUMPER |
            CATEGORY_RIGHT_LOWER_BUMPER |
            CATEGORY_ONE_WAY;
        ballShapeDef.userData = &(game->balls[ballIndex]);

        game->balls[ballIndex].shape = b2CreateCircleShape(game->balls[ballIndex].body, &ballShapeDef, &ballCircle);
        game->balls[ballIndex].active = 1;
        game->balls[ballIndex].game = game;
        game->balls[ballIndex].trailStartIndex = 0;
        game->balls[ballIndex].type = type;
        game->balls[ballIndex].killCounter = 0;
        game->balls[ballIndex].underwaterState = 0;

        if (type == 0) {
            game->slowMotion = 0;
        }

        for (int i = 0; i < 16; i++) {
            game->balls[ballIndex].locationHistoryX[i] = px;
            game->balls[ballIndex].locationHistoryY[i] = py;
        }

        playLaunch(game->sound);
    }
}

/*
 * Helper to draw a Box2D body and its shapes for debug visualization
 */
static void debug_draw_body(b2BodyId bodyId, DebugColor outlineColor, DebugColor fillColor) {
    if (B2_IS_NULL(bodyId)) {
        return;
    }
    
    b2Vec2 pos = b2Body_GetPosition(bodyId);
    b2Rot rot = b2Body_GetRotation(bodyId);
    float angle = b2Rot_GetAngle(rot);
    
    // Get shape IDs for this body
    // Box2D 3.x requires iterating through shape IDs
    // We'll use b2Body_GetShapes to get all shapes
    int shapeCount = b2Body_GetShapeCount(bodyId);
    if (shapeCount == 0) {
        return;
    }
    
    b2ShapeId shapeIds[16]; // Max shapes per body
    int actualCount = b2Body_GetShapes(bodyId, shapeIds, 16);
    
    for (int i = 0; i < actualCount; i++) {
        b2ShapeId shapeId = shapeIds[i];
        if (B2_IS_NULL(shapeId)) {
            continue;
        }
        
        b2ShapeType shapeType = b2Shape_GetType(shapeId);
        
        if (shapeType == b2_circleShape) {
            b2Circle circle = b2Shape_GetCircle(shapeId);
            Vec2 debugPos = {pos.x + circle.center.x, pos.y + circle.center.y};
            ChipmunkDebugDrawCircle(debugPos, angle, circle.radius, outlineColor, fillColor);
        } else if (shapeType == b2_segmentShape) {
            b2Segment segment = b2Shape_GetSegment(shapeId);
            // Transform segment points to world space
            float cosA = cosf(angle);
            float sinA = sinf(angle);
            Vec2 p1 = {
                pos.x + (segment.point1.x * cosA - segment.point1.y * sinA),
                pos.y + (segment.point1.x * sinA + segment.point1.y * cosA)
            };
            Vec2 p2 = {
                pos.x + (segment.point2.x * cosA - segment.point2.y * sinA),
                pos.y + (segment.point2.x * sinA + segment.point2.y * cosA)
            };
            ChipmunkDebugDrawSegment(p1, p2, outlineColor);
        } else if (shapeType == b2_polygonShape) {
            b2Polygon polygon = b2Shape_GetPolygon(shapeId);
            Vec2 verts[B2_MAX_POLYGON_VERTICES];
            float cosA = cosf(angle);
            float sinA = sinf(angle);
            for (int v = 0; v < polygon.count; v++) {
                verts[v].x = pos.x + (polygon.vertices[v].x * cosA - polygon.vertices[v].y * sinA);
                verts[v].y = pos.y + (polygon.vertices[v].x * sinA + polygon.vertices[v].y * cosA);
            }
            ChipmunkDebugDrawPolygon(polygon.count, verts, 0.5f, outlineColor, fillColor);
        }
    }
}

/*
 * physics_debug_draw
 *  - Iterates through all bodies and shapes in the Box2D world and draws them
 *    using the debug draw API defined in physicsDebugDraw.h
 *  - This provides a visual representation of the physics simulation for debugging
 */
void physics_debug_draw(GameStruct *game) {
    if (B2_IS_NULL(game->world)) {
        return;
    }
    
    // Define colors for different shape types
    DebugColor wallColor = {0.6f, 0.6f, 0.6f, 1.0f};      // Light gray
    DebugColor bumperColor = {1.0f, 0.4f, 0.4f, 1.0f};    // Red
    DebugColor paddleColor = {0.4f, 1.0f, 0.4f, 1.0f};    // Green
    DebugColor ballColor = {0.4f, 0.4f, 1.0f, 1.0f};      // Blue
    DebugColor fillColor = {0.2f, 0.2f, 0.2f, 0.3f};      // Semi-transparent fill
    
    // Draw static body (walls and static geometry)
    if (B2_IS_NON_NULL(debugState.staticBody)) {
        debug_draw_body(debugState.staticBody, wallColor, fillColor);
    }
    
    // Draw bumpers
    for (int i = 0; i < debugState.numBumpers; i++) {
        if (B2_IS_NON_NULL(debugState.bumpers[i].body)) {
            debug_draw_body(debugState.bumpers[i].body, bumperColor, fillColor);
        }
    }
    
    // Draw flippers
    if (debugState.leftFlipper != NULL) {
        b2BodyId leftFlipperBody = *debugState.leftFlipper;
        if (B2_IS_NON_NULL(leftFlipperBody)) {
            debug_draw_body(leftFlipperBody, paddleColor, fillColor);
        }
    }
    if (debugState.rightFlipper != NULL) {
        b2BodyId rightFlipperBody = *debugState.rightFlipper;
        if (B2_IS_NON_NULL(rightFlipperBody)) {
            debug_draw_body(rightFlipperBody, paddleColor, fillColor);
        }
    }
    
    // Draw all active balls
    for (int i = 0; i < maxBalls; i++) {
        if (game->balls[i].active && B2_IS_NON_NULL(game->balls[i].body)) {
            debug_draw_body(game->balls[i].body, ballColor, fillColor);
        }
    }
}