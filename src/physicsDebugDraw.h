#ifndef HEADER_PHYSICS_DEBUG_DRAW
#define HEADER_PHYSICS_DEBUG_DRAW

// Minimal debug draw interface used with Box2D-based physics.
// These functions are currently implemented as no-ops in physicsDebugDraw.c
// so they won't break the build while we are focused on Box2D migration.

typedef struct Vec2 {
    float x;
    float y;
} Vec2;

typedef struct DebugColor {
    float r;
    float g;
    float b;
    float a;
} DebugColor;

void ChipmunkDebugDrawCircle(Vec2 pos, float angle, float radius,
                             DebugColor outlineColor, DebugColor fillColor);

void ChipmunkDebugDrawSegment(Vec2 a, Vec2 b, DebugColor color);

void ChipmunkDebugDrawFatSegment(Vec2 a, Vec2 b, float radius,
                                 DebugColor outlineColor, DebugColor fillColor);

void ChipmunkDebugDrawPolygon(int count, const Vec2 *verts, float radius,
                              DebugColor outlineColor, DebugColor fillColor);

void ChipmunkDebugDrawDot(float size, Vec2 pos, DebugColor fillColor);

DebugColor ChipmunkDebugGetColorForShape(void *shape, void *data);

#endif
