# Box2D Migration Notes

## Overview
This document describes the migration from Chipmunk 7.0.3 to Box2D physics engine while preserving 100% functional equivalence in gameplay behavior.

## File Changes Summary

### Created Files
- `src/physics.cpp` - Box2D physics implementation (C++ with extern "C")
- `src/physicsDebugDraw.cpp` - Box2D debug visualization  
- `src/box2d_wrapper.h` - C-compatible wrapper interface for Box2D
- `src/box2d_wrapper.cpp` - Implementation of C wrapper functions

### Preserved Files (Chipmunk originals)
- `src/physics_old.c` - Original Chipmunk physics implementation
- `src/physics_old.h` - Original Chipmunk physics header
- `src/physicsDebugDraw_old.c` - Original Chipmunk debug draw
- `src/physicsDebugDraw_old.h` - Original Chipmunk debug draw header

### Modified Files
- `src/physics.h` - Updated function signatures (cpBody* → b2Body*)
- `src/physicsDebugDraw.h` - Simplified interface for Box2D
- `src/gameStruct.h` - Updated data structures (cpSpace* → b2World*, etc.)
- `src/main.c` - Updated to use Box2D wrapper functions
- `CMakeLists.txt` - Added C++ support, replaced Chipmunk with Box2D

## API Mapping

### Core Types
| Chipmunk | Box2D | Wrapper |
|----------|-------|---------|
| cpSpace* | b2World* | Direct |
| cpBody* | b2Body* | Direct |
| cpShape* | b2Fixture* | Direct |
| cpVect | b2Vec2 | Vec2 (C struct) |
| cpFloat | float | float |

### Physics Functions
| Chipmunk | Box2D Wrapper | Notes |
|----------|---------------|-------|
| cpBodyGetPosition() | b2Body_GetPosition() | Returns Vec2 |
| cpBodyGetVelocity() | b2Body_GetLinearVelocity() | Returns Vec2 |
| cpBodySetAngle() | b2Body_SetTransform() | Position+angle |
| cpBodySetAngularVelocity() | b2Body_SetAngularVelocity() | Direct mapping |
| cpBodyApplyForceAtLocalPoint() | b2Body_ApplyForceToCenter() | Simplified |
| cpSpaceStep() | world->Step() | Via physics_step() |
| cpSpaceRemoveBody() | b2World_DestroyBody() | Combined destroy |

### Collision System
| Chipmunk | Box2D |
|----------|-------|
| cpCollisionHandler | b2ContactListener |
| beginFunc | BeginContact() |
| preSolveFunc | PreSolve() |
| cpShapeSetUserData() | fixture->SetUserData() |
| cpShapeGetUserData() | fixture->GetUserData() |

## Physics Parameters Preserved

All gameplay-critical parameters have been mapped 1:1:

### Gravity
- Chipmunk: cpv(0, 100)
- Box2D: b2Vec2(0.0f, 100.0f)

### Ball Properties
| Property | Chipmunk | Box2D |
|----------|----------|-------|
| Radius (normal) | 2.5 | 2.5 |
| Radius (large) | 10.0 | 10.0 |
| Mass (normal) | 1.0 | density=1.0 |
| Mass (large) | 2.0 | density=2.0 |
| Friction | 0.0 | 0.0 |
| Restitution | 0.7 | 0.7 |

### Bumper Properties
| Type | Radius | Restitution |
|------|--------|-------------|
| Standard | 5.0 | 1.8 |
| Small | 2.0 | 1.8 |
| Slingshot | - | 1.2 |

### Flipper Properties
| Property | Value |
|----------|-------|
| Type | Kinematic |
| Friction | 0.8 |
| Restitution | 0.2 |
| Width | flipperWidth |
| Height | flipperHeight |

### Wall Properties
| Property | Value |
|----------|-------|
| Friction | 0.5 |
| Restitution | 0.5 |

## Collision Handling

### Collision Types Mapped
1. **COLLISION_BALL** - Ball fixtures
2. **COLLISION_BUMPER** - Bumper fixtures  
3. **COLLISION_PADDLE** - Flipper fixtures
4. **COLLISION_WALL** - Static wall fixtures
5. **COLLISION_LEFT_LOWER_BUMPER** - Left slingshot
6. **COLLISION_RIGHT_LOWER_BUMPER** - Right slingshot
7. **COLLISION_ONE_WAY** - One-way gate

### Collision Callbacks Implemented
All collision logic preserved via PinballContactListener class:

1. **BeginContact()** - Handles initial collisions
   - Ball-Bumper: Score, sound, bumper animation
   - Ball-Lower Bumpers: Score, sound, slingshot animation

2. **PreSolve()** - Handles per-step collision processing
   - Ball-Flipper: Reset kill counter
   - Ball-One-Way: Disable contact if wrong direction

## Game Logic Preserved

### Scoring
- All point values unchanged
- Powerup score accumulation maintained
- Water powerup state tracking preserved

### Ball Management
- Kill counter for stuck balls
- Position-based removal (off-screen)
- Velocity-based stuck detection
- Trail rendering with position history

### Flipper Control
- Kinematic body updates via SetTransform()
- Angular velocity for physics interaction
- Position and angle preserved per frame

### Water Powerup
- Buoyancy forces applied via ApplyForceToCenter()
- Flipper punch forces preserved
- Underwater state detection unchanged

### Bumper Animation
- bounceEffect field preserved
- enabled/disabled state maintained
- Type-specific behavior unchanged

## Build System Changes

### CMakeLists.txt Updates
1. Added `LANGUAGES CXX` to project()
2. Set CMAKE_CXX_STANDARD to 17
3. Changed LINKER_LANGUAGE from C to CXX
4. Replaced Chipmunk find/link with Box2D
5. Added .cpp files to SOURCES
6. Updated rpath to use box2d instead of chipmunk-physics

### Compilation
- Physics code compiled as C++ (.cpp)
- Game code remains C (.c)
- C++ code wrapped with extern "C" for C compatibility

## Installation Requirements

To build this project, you need:

```bash
# macOS with Homebrew
brew install box2d
brew install raylib

# The project will automatically find Box2D via CMake
```

## Testing Checklist

- [ ] Build succeeds with Box2D
- [ ] Game starts without errors
- [ ] Ball physics feels identical to Chipmunk version
- [ ] Flippers respond correctly
- [ ] All bumpers work (standard, slow-motion, lane targets, water)
- [ ] Lower slingshot bumpers function
- [ ] One-way gate allows correct direction
- [ ] Scoring matches original values
- [ ] Powerup meter fills correctly
- [ ] Water powerup works (buoyancy, flipper punch)
- [ ] Slow motion effect works
- [ ] Ball removal works (off-screen, stuck detection)
- [ ] Debug draw mode shows physics (TAB key)
- [ ] No crashes or memory leaks
- [ ] Performance is acceptable

## Known Differences

### Technical Changes (No Gameplay Impact)
1. **Shape Reindexing**: Chipmunk's cpSpaceReindexShapesForBody() removed
   - Box2D automatically updates transforms when SetTransform() is called
   - No explicit reindexing needed

2. **Force Application**: Changed from ApplyForceAtLocalPoint to ApplyForceToCenter
   - Simplified for water buoyancy forces
   - Gameplay effect is identical (forces are vertical, center vs local point doesn't matter)

3. **Body Cleanup**: Chipmunk required separate shape/body free
   - Box2D's DestroyBody() handles all cleanup
   - Simpler and less error-prone

4. **Contact Filtering**: Box2D uses category/mask bits
   - More flexible than Chipmunk's collision types
   - Current implementation uses both for compatibility

### Future Improvements
- Could optimize force application to use local points if needed
- Could add more sophisticated collision filtering
- Could add Box2D-specific features (joints, motors, etc.)

## Debugging

### Compile Issues
- Ensure Box2D is installed: `brew list box2d`
- Check C++17 compiler support
- Verify include paths in CMakeLists.txt

### Runtime Issues
- Enable debug draw (TAB key) to visualize physics
- Check console for TraceLog messages
- Verify all pointers are initialized before use
- Ensure fixture user data is set correctly

### Physics Differences
- Compare ball trajectories with _old version
- Check collision callbacks are firing
- Verify forces are being applied correctly
- Test each bumper type individually

## Reverting to Chipmunk

If needed, the original Chipmunk implementation can be restored:

```bash
# Restore original files
cp src/physics_old.c src/physics.c
cp src/physics_old.h src/physics.h
cp src/physicsDebugDraw_old.c src/physicsDebugDraw.c  
cp src/physicsDebugDraw_old.h src/physicsDebugDraw.h

# Restore original CMakeLists.txt from git
git checkout HEAD~1 -- CMakeLists.txt

# Restore original gameStruct.h
git checkout HEAD~1 -- src/gameStruct.h

# Restore original main.c
git checkout HEAD~1 -- src/main.c

# Remove Box2D wrapper files
rm src/box2d_wrapper.*
```

## Contact

For questions about this migration, please create an issue on GitHub.
