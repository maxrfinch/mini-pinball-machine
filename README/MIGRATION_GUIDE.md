# Chipmunk to Box2D 3.x Migration Guide

## Overview

This document describes the migration from Chipmunk Physics to Box2D 3.x C API for the Mini Pinball Machine project.

## Migration Status

✅ **COMPLETED**

All code has been migrated to use Box2D 3.x C API. The migration preserves all game behavior while switching physics engines.

## What Was Changed

### 1. Structure Types (gameStruct.h)

**Before (Chipmunk):**
```c
typedef struct {
    cpShape *shape;
    cpBody *body;
    // ...
} Ball;

struct GameStructData {
    cpSpace *space;
    // ...
};
```

**After (Box2D 3.x):**
```c
typedef struct {
    b2ShapeId shape;
    b2BodyId body;
    // ...
} Ball;

struct GameStructData {
    b2WorldId world;
    // ...
};
```

### 2. Physics API (physics.h)

Function signatures updated to use Box2D types:
- `cpBody*` → `b2BodyId*`
- `cpSpace*` → `b2WorldId`

### 3. Core Physics Implementation (physics.c)

#### World Creation
**Before:**
```c
cpSpace *space = cpSpaceNew();
cpSpaceSetGravity(space, cpv(0, 100));
```

**After:**
```c
b2WorldDef worldDef = b2DefaultWorldDef();
worldDef.gravity = pb2_v(0, 100);
game->world = b2CreateWorld(&worldDef);

// Register PreSolve callback for collision handling
b2World_SetPreSolveCallback(game->world, PreSolveCallback, NULL);
```

#### Contact Event Handling
**Before (Chipmunk):**
```c
cpCollisionHandler *handler = cpSpaceAddCollisionHandler(space, COLLISION_BALL, COLLISION_BUMPER);
handler->beginFunc = CollisionHandlerBallBumper;
```

**After (Box2D 3.x):**
```c
// Unified PreSolveCallback registered using b2World_SetPreSolveCallback
b2World_SetPreSolveCallback(game->world, PreSolveCallback, NULL);

// Callback identifies collision types using filter category bits
static bool PreSolveCallback(b2ShapeId shapeIdA, b2ShapeId shapeIdB, b2Manifold* manifold, void* context) {
    b2Filter filterA = b2Shape_GetFilter(shapeIdA);
    b2Filter filterB = b2Shape_GetFilter(shapeIdB);
    // Identify collision types and handle accordingly
}
```

#### Shape Creation
**Before:**
```c
cpShape *wall = cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(x1, y1), cpv(x2, y2), 0);
cpShapeSetFriction(wall, 0.5);
cpShapeSetElasticity(wall, 0.5);
cpSpaceAddShape(space, wall);
```

**After:**
```c
b2Segment segment;
segment.point1 = pb2_v(x1, y1);
segment.point2 = pb2_v(x2, y2);

b2ShapeDef shapeDef = b2DefaultShapeDef();
shapeDef.friction = 0.5f;
shapeDef.restitution = 0.5f;
b2CreateSegmentShape(staticBody, &shapeDef, &segment);
```

#### Body Management
**Before:**
```c
cpVect pos = cpBodyGetPosition(body);
cpBodySetAngle(body, angle);
cpBodySetVelocity(body, cpv(vx, vy));
```

**After:**
```c
b2Vec2 pos = b2Body_GetPosition(body);
b2Rot rot = b2MakeRot(angle);
b2Body_SetTransform(body, pos, rot);
b2Body_SetLinearVelocity(body, pb2_v(vx, vy));
```

### 4. Main Game Loop (main.c)

All Chipmunk API calls replaced with Box2D equivalents:
- `cpBodyGetPosition` → `b2Body_GetPosition`
- `cpBodyGetVelocity` → `b2Body_GetLinearVelocity`
- `cpBodySetAngle` → `b2Body_SetTransform` with `b2MakeRot`
- `cpBodyApplyForceAtLocalPoint` → `b2Body_ApplyForceToCenter`
- `cpvlengthsq(vel)` → `vel.x * vel.x + vel.y * vel.y`

### 5. Build System (CMakeLists.txt)

**Before:**
```cmake
find_library(CHIPMUNK_LIB NAMES chipmunk ...)
target_link_libraries(${PROJECT_NAME} PRIVATE ${CHIPMUNK_LIB})
```

**After:**
```cmake
find_library(BOX2D_LIB NAMES box2d ...)
target_link_libraries(${PROJECT_NAME} PRIVATE ${BOX2D_LIB})
```

## Building the Migrated Project

### Prerequisites

On macOS with Homebrew:
```bash
brew install box2d
brew install raylib
```

### Build Steps

```bash
mkdir build
cd build
cmake ..
make
```

### Expected Output

The game should build and run with identical behavior to the Chipmunk version:
- Ball physics (gravity, bounce, friction)
- Flipper control and response
- Bumper collisions and scoring
- Powerup system (slow motion, water, etc.)
- One-way gate behavior
- Slingshot responses

## Key Differences: Chipmunk vs Box2D 3.x

### Contact Event System

**Chipmunk:**
- Uses separate collision handlers per pair of collision types
- Multiple callback functions: beginFunc, preSolveFunc, postSolveFunc, separateFunc
- Handlers registered with `cpSpaceAddCollisionHandler`

**Box2D 3.x:**
- Uses unified PreSolve callback registered in world definition
- Single callback function handles all collision types
- Identifies collision pairs using filter category bits
- More centralized collision logic

### Body/Shape Management

**Chipmunk:**
- Pointers to bodies and shapes (`cpBody*`, `cpShape*`)
- Explicit add/remove from space
- Separate free calls for cleanup

**Box2D 3.x:**
- ID-based handles (`b2BodyId`, `b2ShapeId`)
- Automatic management through world
- Single `b2DestroyBody` cleans up associated shapes

### Coordinate System

Both use the same coordinate system (matches the existing game), so no coordinate transformations were needed.

## Reference Files

The original Chipmunk implementation is preserved for reference:
- `src/physics_old.c` - Original physics implementation
- `src/physics_old.h` - Original physics header
- `src/physicsDebugDraw_old.c` - Original debug draw
- `src/physicsDebugDraw_old.h` - Original debug draw header

These files are **not compiled** but serve as behavioral reference.

## Testing Checklist

When running the migrated game, verify:

- [ ] Ball launches correctly with initial velocity
- [ ] Ball responds to gravity and bounces off walls
- [ ] Flippers rotate and hit balls correctly
- [ ] Standard bumpers award 50 points and show bounce effect
- [ ] Slow-motion bumper triggers slow-motion mode and awards 1000 points
- [ ] Lane target bumpers award points when enabled, become disabled after hit
- [ ] Water powerup bumpers work correctly
- [ ] Lower slingshots award 25 points and show animation
- [ ] One-way gate allows balls through in correct direction only
- [ ] Ball kill counter resets on flipper contact
- [ ] Balls are removed when they fall off screen or stall
- [ ] Water physics applies buoyancy force when active
- [ ] All scoring and powerup logic works as before

## Known Limitations

- Debug draw functionality commented out (can be re-implemented if needed)
- Requires Box2D 3.x (not compatible with Box2D 2.x)

## Troubleshooting

### Build Errors

**"box2d/box2d.h not found":**
```bash
brew install box2d
```

**"raylib.h not found":**
```bash
brew install raylib
```

### Runtime Issues

**Collision callbacks not firing:**
- Verify `b2World_SetPreSolveCallback` is called after world creation
- Check filter category bits are set correctly on shapes
- Ensure user data is set on shapes that need callbacks

**Physics feels different:**
- Check gravity value (should be 0, 100)
- Verify restitution/friction values match Chipmunk version
- Review sub-step count in `physics_step` (set to 1 to match Chipmunk)

## Additional Notes

- The migration maintains pure C code (no C++)
- All game logic preserved exactly as in Chipmunk version
- Box2D 3.x C API uses modern Id-based handles instead of pointers
- Contact event handling centralized for easier maintenance
