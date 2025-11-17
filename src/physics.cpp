/*
 * physics.cpp
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
 * Migration from Chipmunk to Box2D:
 *  - Replaced cpSpace with b2World
 *  - Replaced cpBody with b2Body
 *  - Replaced cpShape with b2Fixture
 *  - Replaced collision handlers with b2ContactListener
 *  - All gameplay behavior preserved 1:1
 */

#include <box2d/box2d.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

extern "C" {
#include "physics.h"
#include "constants.h"
#include "soundManager.h"
#include "raylib.h"
}

#define DEG_TO_RAD (3.14159265f / 180.0f)

// Local geometry/limit constants for this module.
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

// Collision categories for Box2D filtering
enum CollisionCategories {
    CATEGORY_WALL   = 0x0001,
    CATEGORY_BALL   = 0x0002,
    CATEGORY_BUMPER = 0x0004,
    CATEGORY_PADDLE = 0x0008,
    CATEGORY_ONE_WAY = 0x0010,
};

// User data types for collision identification
enum CollisionTypes {
    COLLISION_WALL = 0,
    COLLISION_BALL = 1,
    COLLISION_BUMPER = 2,
    COLLISION_PADDLE = 3,
    COLLISION_LEFT_LOWER_BUMPER = 4,
    COLLISION_RIGHT_LOWER_BUMPER = 5,
    COLLISION_ONE_WAY = 6
};

// User data structure to identify collision types
struct CollisionUserData {
    int collisionType;
    void* ptr;  // Points to Ball* or Bumper*
};

/*
 * PinballContactListener
 *  - Implements Box2D collision callbacks to replicate Chipmunk collision handlers
 */
class PinballContactListener : public b2ContactListener {
public:
    void BeginContact(b2Contact* contact) override {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        
        CollisionUserData* dataA = (CollisionUserData*)fixtureA->GetUserData().pointer;
        CollisionUserData* dataB = (CollisionUserData*)fixtureB->GetUserData().pointer;
        
        if (!dataA || !dataB) return;
        
        // Ball-Bumper collision
        if ((dataA->collisionType == COLLISION_BALL && dataB->collisionType == COLLISION_BUMPER) ||
            (dataA->collisionType == COLLISION_BUMPER && dataB->collisionType == COLLISION_BALL)) {
            
            Ball* ball = nullptr;
            Bumper* bumper = nullptr;
            
            if (dataA->collisionType == COLLISION_BALL) {
                ball = (Ball*)dataA->ptr;
                bumper = (Bumper*)dataB->ptr;
            } else {
                ball = (Ball*)dataB->ptr;
                bumper = (Bumper*)dataA->ptr;
            }
            
            if (!ball || !bumper) return;
            
            HandleBallBumperCollision(ball, bumper, contact);
        }
        
        // Ball-Left Lower Bumper
        else if ((dataA->collisionType == COLLISION_BALL && dataB->collisionType == COLLISION_LEFT_LOWER_BUMPER) ||
                 (dataA->collisionType == COLLISION_LEFT_LOWER_BUMPER && dataB->collisionType == COLLISION_BALL)) {
            
            Ball* ball = (dataA->collisionType == COLLISION_BALL) ? (Ball*)dataA->ptr : (Ball*)dataB->ptr;
            if (!ball) return;
            
            leftLowerBumperAnim = 1.0f;
            (ball->game)->gameScore += 25;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 25;
            }
            playBounce2((ball->game)->sound);
        }
        
        // Ball-Right Lower Bumper
        else if ((dataA->collisionType == COLLISION_BALL && dataB->collisionType == COLLISION_RIGHT_LOWER_BUMPER) ||
                 (dataA->collisionType == COLLISION_RIGHT_LOWER_BUMPER && dataB->collisionType == COLLISION_BALL)) {
            
            Ball* ball = (dataA->collisionType == COLLISION_BALL) ? (Ball*)dataA->ptr : (Ball*)dataB->ptr;
            if (!ball) return;
            
            rightLowerBumperAnim = 1.0f;
            (ball->game)->gameScore += 25;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 25;
            }
            playBounce2((ball->game)->sound);
        }
    }
    
    void PreSolve(b2Contact* contact, const b2Manifold* oldManifold) override {
        b2Fixture* fixtureA = contact->GetFixtureA();
        b2Fixture* fixtureB = contact->GetFixtureB();
        
        CollisionUserData* dataA = (CollisionUserData*)fixtureA->GetUserData().pointer;
        CollisionUserData* dataB = (CollisionUserData*)fixtureB->GetUserData().pointer;
        
        if (!dataA || !dataB) return;
        
        // Ball-Flipper collision - reset kill counter
        if ((dataA->collisionType == COLLISION_BALL && dataB->collisionType == COLLISION_PADDLE) ||
            (dataA->collisionType == COLLISION_PADDLE && dataB->collisionType == COLLISION_BALL)) {
            
            Ball* ball = (dataA->collisionType == COLLISION_BALL) ? (Ball*)dataA->ptr : (Ball*)dataB->ptr;
            if (ball) {
                ball->killCounter = 0;
            }
        }
        
        // One-way collision
        else if ((dataA->collisionType == COLLISION_BALL && dataB->collisionType == COLLISION_ONE_WAY) ||
                 (dataA->collisionType == COLLISION_ONE_WAY && dataB->collisionType == COLLISION_BALL)) {
            
            b2WorldManifold worldManifold;
            contact->GetWorldManifold(&worldManifold);
            b2Vec2 normal = worldManifold.normal;
            
            // Check if ball is moving in disallowed direction
            if (normal.y < 0) {
                contact->SetEnabled(false);
            }
        }
    }
    
private:
    void HandleBallBumperCollision(Ball* ball, Bumper* bumper, b2Contact* contact) {
        if (bumper->type == BUMPER_TYPE_STANDARD) {
            // Standard upper playfield bumper: visual bounce + modest score.
            bumper->bounceEffect = 10.0f;
            (ball->game)->gameScore += 50;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 50;
            }
            playUpperBouncerSound((ball->game)->sound);
        } else if (bumper->type == BUMPER_TYPE_SLOW_MOTION) {
            // Special bumper that triggers slow-motion mode and a large point bonus.
            (ball->game)->slowMotion = 1;
            (ball->game)->slowMotionCounter = 1200;
            (ball->game)->gameScore += 1000;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 1000;
            }
            playSlowdownSound((ball->game)->sound);
            bumper->bounceEffect = 20.0f;
        } else if (bumper->type == BUMPER_TYPE_LANE_TARGET_A ||
                   bumper->type == BUMPER_TYPE_LANE_TARGET_B) {
            // Lane/target bumpers: only award score once while enabled.
            if (bumper->enabled == 1) {
                (ball->game)->gameScore += 50;
                if ((ball->game)->waterPowerupState == 0) {
                    (ball->game)->powerupScore += 50;
                }
                bumper->enabled = 0;
                playBounce((ball->game)->sound);
            }
        } else if (bumper->type == BUMPER_TYPE_WATER_POWERUP) {
            // Small bumpers that drive the water powerup state.
            if (bumper->enabled == 1) {
                bumper->bounceEffect = 10.0f;
                (ball->game)->gameScore += 250;
                if ((ball->game)->waterPowerupState == 0) {
                    (ball->game)->powerupScore += 250;
                }
                bumper->enabled = 0;
                playBounce((ball->game)->sound);
            } else {
                contact->SetEnabled(false);
            }
        } else {
            // Fallback: treat as a simple one-shot scoring bumper.
            (ball->game)->gameScore += 25;
            if ((ball->game)->waterPowerupState == 0) {
                (ball->game)->powerupScore += 25;
            }
            bumper->enabled = 0;
        }
    }
};

// Global contact listener instance
static PinballContactListener* g_contactListener = nullptr;

/*
 * writeCircleWallSegment
 *  - Fills a contiguous range of entries in the walls[][] array with short line
 *    segments that approximate a circular arc.
 */
static void writeCircleWallSegment(float walls[numWalls][4], int segmentIndex, int numSegments,
                                   float degStart, float degEnd, float centerX, float centerY, float radius) {
    float totalDegreeRange = fabsf(degEnd - degStart);
    float degPerPoint = totalDegreeRange / (numSegments + 1);

    degStart -= 90;

    float prevPointX = centerX + (cosf(degStart * DEG_TO_RAD) * radius);
    float prevPointY = centerY + (sinf(degStart * DEG_TO_RAD) * radius);
    float curPointX = 0;
    float curPointY = 0;
    for (int i = 1; i <= numSegments; i++) {
        curPointX = centerX + (cosf((degStart + (i * degPerPoint)) * DEG_TO_RAD) * radius);
        curPointY = centerY + (sinf((degStart + (i * degPerPoint)) * DEG_TO_RAD) * radius);
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
extern "C" void physics_init(GameStruct *game, Bumper **out_bumpers, b2Body **out_leftFlipperBody, b2Body **out_rightFlipperBody) {
    /* ----------------------------- Static walls ----------------------------- */
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

    // Initialize Box2D world with gravity matching Chipmunk
    b2Vec2 gravity(0.0f, 100.0f);
    b2World* world = new b2World(gravity);
    game->world = world;
    
    // Create and register contact listener
    g_contactListener = new PinballContactListener();
    world->SetContactListener(g_contactListener);

    // Create static body for walls
    b2BodyDef staticBodyDef;
    staticBodyDef.type = b2_staticBody;
    staticBodyDef.position.Set(0.0f, 0.0f);
    b2Body* staticBody = world->CreateBody(&staticBodyDef);

    // Create walls
    for (int i = 0; i < numWalls; i++) {
        b2Vec2 v1(walls[i][0], walls[i][1]);
        b2Vec2 v2(walls[i][2], walls[i][3]);
        
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.friction = 0.5f;
        fixtureDef.restitution = 0.5f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        fixtureDef.filter.maskBits = 0xFFFF;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }

    /* ------------------------------ Bumpers --------------------------------- */
    const float bumperSize = 10.0f;
    const float smallBumperSize = 4.0f;
    const float bumperBounciness = 1.8f;
    Bumper* bumpers = (Bumper*)malloc(numBumpers * sizeof(Bumper));

    // Lower slingshot bumpers
    {
        b2Vec2 v1(14.800000f, 125.200005f);
        b2Vec2 v2(7.600000f, 109.200005f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.friction = 0.0f;
        fixtureDef.restitution = 1.2f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_LEFT_LOWER_BUMPER;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }
    
    {
        b2Vec2 v1(75.599998f, 108.800003f);
        b2Vec2 v2(69.200005f, 125.200005f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.friction = 0.0f;
        fixtureDef.restitution = 1.2f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_RIGHT_LOWER_BUMPER;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }

    // Bouncer guards
    {
        b2Vec2 v1(7.200000f, 111.200005f);
        b2Vec2 v2(12.800000f, 124.400002f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.friction = 0.0f;
        fixtureDef.restitution = 0.9f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }
    
    {
        b2Vec2 v1(71.200005f, 124.800003f);
        b2Vec2 v2(76.000000f, 110.800003f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.friction = 0.0f;
        fixtureDef.restitution = 0.9f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }

    // Create circular bumpers
    auto createBumper = [&](int index, float x, float y, float radius, int type) {
        b2BodyDef bodyDef;
        bodyDef.type = b2_kinematicBody;
        bodyDef.position.Set(x, y);
        bumpers[index].body = world->CreateBody(&bodyDef);
        
        b2CircleShape circleShape;
        circleShape.m_radius = radius;
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circleShape;
        fixtureDef.restitution = bumperBounciness;
        fixtureDef.filter.categoryBits = CATEGORY_BUMPER;
        fixtureDef.filter.maskBits = 0xFFFF;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_BUMPER;
        userData->ptr = &bumpers[index];
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        bumpers[index].fixture = bumpers[index].body->CreateFixture(&fixtureDef);
        bumpers[index].bounceEffect = 0;
        bumpers[index].type = type;
        bumpers[index].enabled = 1;
    };

    // Standard bumpers
    createBumper(0, 24.9f, 19.9f, bumperSize/2.0f, BUMPER_TYPE_STANDARD);
    createBumper(1, 46.6f, 17.8f, bumperSize/2.0f, BUMPER_TYPE_STANDARD);
    createBumper(2, 38.0f, 36.4f, bumperSize/2.0f, BUMPER_TYPE_STANDARD);
    
    // Slow motion bumper
    createBumper(3, 72.200005f, 23.400000f, 2.0f, BUMPER_TYPE_SLOW_MOTION);
    
    // Lane target bumpers
    for (int i = 4; i < 10; i++) {
        createBumper(i, 0, 0, 2.0f, BUMPER_TYPE_LANE_TARGET_A);
    }
    
    bumpers[4].body->SetTransform(b2Vec2(63.34f, 50.88f), 0);
    bumpers[5].body->SetTransform(b2Vec2(77.38f, 70.96f), 0);
    bumpers[6].body->SetTransform(b2Vec2(15.1f, 62.04f), 0);
    bumpers[7].body->SetTransform(b2Vec2(18.9f, 45.3f), 0);
    bumpers[8].body->SetTransform(b2Vec2(61.02f, 35.36f), 0);
    bumpers[9].body->SetTransform(b2Vec2(65.02f, 23.02f), 0);
    
    bumpers[4].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[5].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[6].type = BUMPER_TYPE_LANE_TARGET_A;
    bumpers[7].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[8].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[9].type = BUMPER_TYPE_LANE_TARGET_B;
    bumpers[4].angle = 90.0f + 145.2f;
    bumpers[5].angle = 90.0f + 145.2f;
    bumpers[6].angle = 90.0f + 25.7f;
    bumpers[7].angle = 90.0f;
    bumpers[8].angle = 90.0f - 162.0f;
    bumpers[9].angle = 90.0f - 162.0f;
    
    // Water powerup bumpers
    createBumper(10, 12.2f, 81.8f, smallBumperSize/2.0f, BUMPER_TYPE_WATER_POWERUP);
    createBumper(11, 23.8f, 91.2f, smallBumperSize/2.0f, BUMPER_TYPE_WATER_POWERUP);
    createBumper(12, 61.2f, 91.2f, smallBumperSize/2.0f, BUMPER_TYPE_WATER_POWERUP);
    createBumper(13, 72.599998f, 81.8f, smallBumperSize/2.0f, BUMPER_TYPE_WATER_POWERUP);
    
    for (int i = 10; i < 14; i++) {
        bumpers[i].enabledSize = 0.0f;
        bumpers[i].enabled = 0;
    }

    // One-way door
    {
        b2Vec2 v1(69.6f, 16.6f);
        b2Vec2 v2(73.4f, 4.6f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.restitution = 0.5f;
        fixtureDef.friction = 0.0f;
        fixtureDef.filter.categoryBits = CATEGORY_ONE_WAY;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_ONE_WAY;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }

    // Additional wall segments
    {
        b2Vec2 v1(7.800000f, 38.200001f);
        b2Vec2 v2(7.8f, 49.200001f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.restitution = 0.5f;
        fixtureDef.friction = 0.5f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }
    
    {
        b2Vec2 v1(16.000000f, 38.400002f);
        b2Vec2 v2(16.000000f, 53.799999f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.restitution = 0.5f;
        fixtureDef.friction = 0.5f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }
    
    {
        b2Vec2 v1(16.000000f, 53.799999f);
        b2Vec2 v2(8.600000f, 68.800003f);
        b2EdgeShape edgeShape;
        edgeShape.SetTwoSided(v1, v2);
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &edgeShape;
        fixtureDef.restitution = 0.5f;
        fixtureDef.friction = 0.5f;
        fixtureDef.filter.categoryBits = CATEGORY_WALL;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_WALL;
        userData->ptr = nullptr;
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        staticBody->CreateFixture(&fixtureDef);
    }

    /* ------------------------------ Flippers -------------------------------- */
    // Create left and right flippers as kinematic bodies
    b2BodyDef leftFlipperBodyDef;
    leftFlipperBodyDef.type = b2_kinematicBody;
    leftFlipperBodyDef.position.Set(19.8f, 145.45f);
    b2Body* leftFlipperBody = world->CreateBody(&leftFlipperBodyDef);
    
    b2BodyDef rightFlipperBodyDef;
    rightFlipperBodyDef.type = b2_kinematicBody;
    rightFlipperBodyDef.position.Set(63.5f, 145.45f);
    b2Body* rightFlipperBody = world->CreateBody(&rightFlipperBodyDef);
    
    // Create flipper polygon shape
    b2Vec2 flipperPoly[4] = {
        b2Vec2(0, 0),
        b2Vec2(flipperWidth, 0),
        b2Vec2(flipperWidth, flipperHeight),
        b2Vec2(0, flipperHeight)
    };
    
    b2PolygonShape flipperShape;
    flipperShape.Set(flipperPoly, 4);
    
    b2FixtureDef leftFlipperFixtureDef;
    leftFlipperFixtureDef.shape = &flipperShape;
    leftFlipperFixtureDef.friction = 0.8f;
    leftFlipperFixtureDef.restitution = 0.2f;
    leftFlipperFixtureDef.filter.categoryBits = CATEGORY_PADDLE;
    leftFlipperFixtureDef.filter.maskBits = 0xFFFF;
    
    CollisionUserData* leftFlipperUserData = new CollisionUserData();
    leftFlipperUserData->collisionType = COLLISION_PADDLE;
    leftFlipperUserData->ptr = nullptr;
    leftFlipperFixtureDef.userData.pointer = (uintptr_t)leftFlipperUserData;
    
    leftFlipperBody->CreateFixture(&leftFlipperFixtureDef);
    
    b2FixtureDef rightFlipperFixtureDef;
    rightFlipperFixtureDef.shape = &flipperShape;
    rightFlipperFixtureDef.friction = 0.8f;
    rightFlipperFixtureDef.restitution = 0.2f;
    rightFlipperFixtureDef.filter.categoryBits = CATEGORY_PADDLE;
    rightFlipperFixtureDef.filter.maskBits = 0xFFFF;
    
    CollisionUserData* rightFlipperUserData = new CollisionUserData();
    rightFlipperUserData->collisionType = COLLISION_PADDLE;
    rightFlipperUserData->ptr = nullptr;
    rightFlipperFixtureDef.userData.pointer = (uintptr_t)rightFlipperUserData;
    
    rightFlipperBody->CreateFixture(&rightFlipperFixtureDef);

    // Return bumpers and flipper bodies to caller
    *out_bumpers = bumpers;
    *out_leftFlipperBody = leftFlipperBody;
    *out_rightFlipperBody = rightFlipperBody;
}

/*
 * physics_step
 *  - Advance the physics simulation by dt seconds.
 */
extern "C" void physics_step(GameStruct *game, float dt) {
    TraceLog(LOG_INFO, "[PHYSICS] stepping dt=%f", dt);
    if (game->world) {
        // Box2D recommends fixed time steps and velocity/position iterations
        const int velocityIterations = 8;
        const int positionIterations = 3;
        game->world->Step(dt, velocityIterations, positionIterations);
    }
    TraceLog(LOG_INFO, "[PHYSICS] done");
}

/*
 * physics_shutdown
 *  - Frees the Box2D world owned by this GameStruct.
 */
extern "C" void physics_shutdown(GameStruct *game) {
    if (game->world != nullptr) {
        delete game->world;
        game->world = nullptr;
    }
    if (g_contactListener != nullptr) {
        delete g_contactListener;
        g_contactListener = nullptr;
    }
}

/*
 * physics_add_ball
 *  - Spawns a new ball into the physics world, if there is capacity.
 */
extern "C" void physics_add_ball(GameStruct *game, float px, float py, float vx, float vy, int type) {
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

        float radius = ballSize / 2.0f;
        float density = 1.0f;
        if (type == 2) {
            radius = 10.0f;
            density = 2.0f;
        }
        
        // Create ball body
        b2BodyDef bodyDef;
        bodyDef.type = b2_dynamicBody;
        bodyDef.position.Set(px, py);
        bodyDef.linearVelocity.Set(vx, vy);
        game->balls[ballIndex].body = game->world->CreateBody(&bodyDef);
        
        // Create ball fixture
        b2CircleShape circleShape;
        circleShape.m_radius = radius;
        
        b2FixtureDef fixtureDef;
        fixtureDef.shape = &circleShape;
        fixtureDef.density = density;
        fixtureDef.friction = 0.0f;
        fixtureDef.restitution = 0.7f;
        fixtureDef.filter.categoryBits = CATEGORY_BALL;
        fixtureDef.filter.maskBits = 0xFFFF;
        
        CollisionUserData* userData = new CollisionUserData();
        userData->collisionType = COLLISION_BALL;
        userData->ptr = &(game->balls[ballIndex]);
        fixtureDef.userData.pointer = (uintptr_t)userData;
        
        game->balls[ballIndex].fixture = game->balls[ballIndex].body->CreateFixture(&fixtureDef);
        game->balls[ballIndex].active = 1;
        game->balls[ballIndex].game = game;
        game->balls[ballIndex].trailStartIndex = 0;
        game->balls[ballIndex].type = type;
        game->balls[ballIndex].killCounter = 0;
        
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
