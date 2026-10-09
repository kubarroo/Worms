#pragma once
#include <SDL_image.h>
#include <box2d/b2_math.h>
#include <optional>
#include <span>
#include <vector>


struct PhysicTexture
{
    std::vector<std::vector<b2Vec2>> points;
    SDL_Surface* surface = nullptr;
};

std::optional<PhysicTexture> IMG_LoadPhysicTexture(SDL_Renderer* renderer, const char* file);
void DrawPolygon(SDL_Renderer* renderer, const std::vector<std::vector<b2Vec2>>& lines,
                 const b2Vec2 pos);
std::vector<std::vector<SDL_Point>> MarchingSquares(Uint32* org_pixels, int w, int h,
                                                    int threshold = 20);
double PointLineDistance(const b2Vec2 point, const b2Vec2 line1, const b2Vec2 line2);
std::vector<b2Vec2> DouglasPeucker(std::span<b2Vec2> points, const double epsilon);
