#include "Core/Physics/b2ColliderDraw.h"
#include <vector>
#include <array>

#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

void b2ColliderDraw::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount,
                                      const b2Color& color)
{
    if (vertexCount <= 0) return;
    std::vector<RenderPoint> points(vertexCount + 1);
    for (int i = 0; i < vertexCount; i++)
    {
        points[i].x = static_cast<int>(SCREEN_WIDTH / 2 + 100.0 * (vertices[i].x - camera.X()));
        points[i].y = static_cast<int>(SCREEN_HEIGHT / 2 - 100.0 * (vertices[i].y - camera.Y()));
    }
    points[vertexCount].x =
        static_cast<int>(SCREEN_WIDTH / 2 + 100.0 * (vertices[0].x - camera.X()));
    points[vertexCount].y =
        static_cast<int>(SCREEN_HEIGHT / 2 - 100.0 * (vertices[0].y - camera.Y()));

    renderer.DrawLines(points, {0, 255, 0, 255});
}

void b2ColliderDraw::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    RenderPoint point1(static_cast<int>(SCREEN_WIDTH / 2 + 100.0 * (p1.x - camera.X())),
                     static_cast<int>(SCREEN_HEIGHT / 2 - 100.0 * (p1.y - camera.Y())));
    RenderPoint point2(static_cast<int>(SCREEN_WIDTH / 2 + 100.0 * (p2.x - camera.X())),
                     static_cast<int>(SCREEN_HEIGHT / 2 - 100.0 * (p2.y - camera.Y())));
    renderer.DrawLine(point1, point2, {0, 255, 0, 255});
}

void b2ColliderDraw::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis,
                                     const b2Color& color)
{
    constexpr int RES = 20;
    std::array<RenderPoint, RES + 1> points;

    for (int i = 0; i <= RES; i++)
    {
        RenderPoint pixelPoint = {static_cast<int>((center.x - camera.X()) * 100 +
                                                 radius * 100 * cos(i * 2.f * M_PI / RES)),
                                static_cast<int>((center.y - camera.Y()) * 100 +
                                                 radius * 100 * sin(i * 2.f * M_PI / RES))};
        RenderPoint screenPoint(SCREEN_WIDTH / 2 + (pixelPoint.x),
                              SCREEN_HEIGHT / 2 - (pixelPoint.y));
        points[i] = screenPoint;
    }

    renderer.DrawLines(points, {0, 255, 0, 255});
}
