#include "raylib.h"
#include <math.h>
#include "constants.h"
#include <stddef.h>
#include "physicsDebugDraw.h"


// Convert our generic DebugColor into a raylib Color
static inline Color DebugColorToRaylib(DebugColor c) {
    unsigned char r = (unsigned char)fminf(fmaxf(c.r * 255.0f, 0.0f), 255.0f);
    unsigned char g = (unsigned char)fminf(fmaxf(c.g * 255.0f, 0.0f), 255.0f);
    unsigned char b = (unsigned char)fminf(fmaxf(c.b * 255.0f, 0.0f), 255.0f);
    unsigned char a = (unsigned char)fminf(fmaxf(c.a * 255.0f, 0.0f), 255.0f);
    return (Color){ r, g, b, a };
}

// Basic stand‑in: we ignore the shape/data details for now and
// just give everything a neutral gray color.
DebugColor ChipmunkDebugGetColorForShape(void *shape, void *data) {
    (void)shape;
    (void)data;

    DebugColor color;
    color.r = 0x93 / 255.0f;
    color.g = 0xa1 / 255.0f;
    color.b = 0xa1 / 255.0f;
    color.a = 1.0f;
    return color;
}

void ChipmunkDebugDrawCircle(Vec2 pos, float angle, float radius,
                             DebugColor outlineColor, DebugColor fillColor) {
    (void)angle; // not used, but kept for signature compatibility

    float sx = pos.x * worldToScreen;
    float sy = pos.y * worldToScreen;
    float rr = radius * worldToScreen;

    DrawCircle((int)sx, (int)sy, rr, DebugColorToRaylib(fillColor));
    DrawCircleLines((int)sx, (int)sy, rr, DebugColorToRaylib(outlineColor));
}

void ChipmunkDebugDrawSegment(Vec2 a, Vec2 b, DebugColor color) {
    Vector2 pa = { a.x * worldToScreen, a.y * worldToScreen };
    Vector2 pb = { b.x * worldToScreen, b.y * worldToScreen };

    // Use a small thickness so segments remain visible at all zoom levels.
    DrawLineEx(pa, pb, 1.0f, DebugColorToRaylib(color));
}

void ChipmunkDebugDrawFatSegment(Vec2 a, Vec2 b, float radius,
                                 DebugColor outlineColor, DebugColor fillColor) {
    Vector2 pa = { a.x * worldToScreen, a.y * worldToScreen };
    Vector2 pb = { b.x * worldToScreen, b.y * worldToScreen };
    float thickness = radius * worldToScreen;

    // Filled "tube"
    DrawLineEx(pa, pb, thickness, DebugColorToRaylib(fillColor));

    // Caps at each end
    DrawCircle((int)pa.x, (int)pa.y, thickness * 0.5f, DebugColorToRaylib(fillColor));
    DrawCircle((int)pb.x, (int)pb.y, thickness * 0.5f, DebugColorToRaylib(fillColor));

    // Outline rings for the caps
    DrawCircleLines((int)pa.x, (int)pa.y, thickness * 0.5f, DebugColorToRaylib(outlineColor));
    DrawCircleLines((int)pb.x, (int)pb.y, thickness * 0.5f, DebugColorToRaylib(outlineColor));
}

void ChipmunkDebugDrawPolygon(int count, const Vec2 *verts, float radius,
                              DebugColor outlineColor, DebugColor fillColor) {
    if (count <= 0 || verts == NULL) return;

    // Fill: draw as a triangle fan around vertex 0
    if (count >= 3) {
        for (int i = 1; i < count - 1; ++i) {
            Vector2 p0 = { verts[0].x * worldToScreen, verts[0].y * worldToScreen };
            Vector2 p1 = { verts[i].x * worldToScreen, verts[i].y * worldToScreen };
            Vector2 p2 = { verts[i + 1].x * worldToScreen, verts[i + 1].y * worldToScreen };

            Vector2 tri[3] = { p0, p1, p2 };
            DrawTriangleFan(tri, 3, DebugColorToRaylib(fillColor));
        }
    }

    // Outline segments
    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        Vec2 a = verts[i];
        Vec2 b = verts[j];

        Vector2 pa = { a.x * worldToScreen, a.y * worldToScreen };
        Vector2 pb = { b.x * worldToScreen, b.y * worldToScreen };

        DrawLineEx(pa, pb, fmaxf(radius * worldToScreen, 1.0f), DebugColorToRaylib(outlineColor));
    }

    // Optional: draw small dots at each vertex for clarity
    for (int i = 0; i < count; ++i) {
        Vec2 v = verts[i];
        ChipmunkDebugDrawDot(1.0f, v, outlineColor);
    }
}

void ChipmunkDebugDrawDot(float size, Vec2 pos, DebugColor fillColor) {
    float sx = pos.x * worldToScreen;
    float sy = pos.y * worldToScreen;
    float r  = (size * worldToScreen) * 0.5f;

    DrawCircle((int)sx, (int)sy, r, DebugColorToRaylib(fillColor));
}
