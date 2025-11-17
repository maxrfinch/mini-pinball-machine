/*
 * physicsDebugDraw.cpp
 *
 * Box2D debug visualization using raylib
 * Replaces Chipmunk debug draw functionality
 */

#include <box2d/box2d.h>
#include <stdio.h>
#include <math.h>

extern "C" {
#include "raylib.h"
#include "constants.h"
#include "physicsDebugDraw.h"
}

/*
 * Box2DDebugDraw
 *  - Custom debug draw class for Box2D that renders using raylib
 */
class Box2DDebugDraw : public b2Draw {
public:
    Box2DDebugDraw() {
        SetFlags(e_shapeBit | e_jointBit | e_aabbBit | e_pairBit | e_centerOfMassBit);
    }
    
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override {
        for (int i = 0; i < vertexCount; i++) {
            int j = (i + 1) % vertexCount;
            DrawLine(
                vertices[i].x * worldToScreen, vertices[i].y * worldToScreen,
                vertices[j].x * worldToScreen, vertices[j].y * worldToScreen,
                ColorFromB2Color(color)
            );
        }
    }
    
    void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override {
        // Draw filled polygon (simplified - just draw outline for now)
        DrawPolygon(vertices, vertexCount, color);
    }
    
    void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override {
        DrawCircleLines(
            center.x * worldToScreen, center.y * worldToScreen,
            radius * worldToScreen,
            ColorFromB2Color(color)
        );
    }
    
    void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override {
        Color raylibColor = ColorFromB2Color(color);
        DrawCircle(
            center.x * worldToScreen, center.y * worldToScreen,
            radius * worldToScreen,
            Fade(raylibColor, 0.5f)
        );
        DrawCircleLines(
            center.x * worldToScreen, center.y * worldToScreen,
            radius * worldToScreen,
            raylibColor
        );
        
        // Draw axis line
        b2Vec2 p = center + radius * axis;
        DrawLine(
            center.x * worldToScreen, center.y * worldToScreen,
            p.x * worldToScreen, p.y * worldToScreen,
            raylibColor
        );
    }
    
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override {
        DrawLine(
            p1.x * worldToScreen, p1.y * worldToScreen,
            p2.x * worldToScreen, p2.y * worldToScreen,
            ColorFromB2Color(color)
        );
    }
    
    void DrawTransform(const b2Transform& xf) override {
        const float k_axisScale = 0.4f;
        b2Vec2 p1 = xf.p;
        
        b2Vec2 p2 = p1 + k_axisScale * xf.q.GetXAxis();
        DrawLine(
            p1.x * worldToScreen, p1.y * worldToScreen,
            p2.x * worldToScreen, p2.y * worldToScreen,
            RED
        );
        
        p2 = p1 + k_axisScale * xf.q.GetYAxis();
        DrawLine(
            p1.x * worldToScreen, p1.y * worldToScreen,
            p2.x * worldToScreen, p2.y * worldToScreen,
            GREEN
        );
    }
    
    void DrawPoint(const b2Vec2& p, float size, const b2Color& color) override {
        DrawCircle(
            p.x * worldToScreen, p.y * worldToScreen,
            size * worldToScreen,
            ColorFromB2Color(color)
        );
    }
    
private:
    Color ColorFromB2Color(const b2Color& color) {
        return (Color){
            (unsigned char)(color.r * 255),
            (unsigned char)(color.g * 255),
            (unsigned char)(color.b * 255),
            (unsigned char)(color.a * 255)
        };
    }
};

// Global debug draw instance
static Box2DDebugDraw* g_debugDraw = nullptr;

/*
 * Box2DDebugDrawWorld
 *  - Main entry point for debug visualization
 *  - Called from rendering code to draw the physics world
 */
extern "C" void Box2DDebugDrawWorld(b2World* world) {
    if (!world) return;
    
    // Create debug draw instance if it doesn't exist
    if (!g_debugDraw) {
        g_debugDraw = new Box2DDebugDraw();
        world->SetDebugDraw(g_debugDraw);
    }
    
    // Draw the world
    world->DebugDraw();
}
