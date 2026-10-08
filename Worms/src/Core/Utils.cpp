#include "Core/Utils.h"

#include <utility>

std::optional<PhysicTexture> IMG_LoadPhysicTexture(SDL_Renderer* renderer, const char* file)
{
    SDL_Texture* texture = NULL;
    SDL_Surface* surface = IMG_Load(file);
    if (surface)
    {
        // texture = SDL_CreateTextureFromSurface( renderer, surface );
        SDL_LockSurface(surface);
        auto points = MarchingSquares((Uint32*)surface->pixels, surface->w, surface->h, 64);
        std::vector<std::vector<b2Vec2>> physPoints;
        for (int i = 0; i < points.size(); i++)
        {
            physPoints.push_back({});
            for (int j = static_cast<int>(points[i].size() - 1); j >= 0; j--)
                physPoints[i].emplace_back(float(points[i][j].x / 100.f),
                                           float(-points[i][j].y / 100.f));
        }
        SDL_UnlockSurface(surface);
        return PhysicTexture{physPoints, surface};
    }

    return {};
}

std::vector<std::vector<SDL_Point>> MarchingSquares(Uint32* org_pixels, int w, int h, int threshold)
{
    std::vector<std::vector<SDL_Point>> shapes{};
    SDL_Surface* bitmap =
        SDL_CreateRGBSurfaceWithFormat(0, w + 1, h + 1, 1, SDL_PIXELFORMAT_INDEX8);

    SDL_LockSurface(bitmap);

    // First and last column and row should be 0
    uint8_t* bitmap_pixels = (uint8_t*)bitmap->pixels;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if ((org_pixels[x + y * w] & 0xFF00000) != 0x00000000)
            {
                bitmap_pixels[x + y * bitmap->w]++;
                bitmap_pixels[x + 1 + y * bitmap->w] += 2;
                bitmap_pixels[x + 1 + (y + 1) * bitmap->w] += 4;
                bitmap_pixels[x + (y + 1) * bitmap->w] += 8;
            }
        }
    }

    int dx = 0, dy = 0;
    int shapeId = -1;
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (bitmap_pixels[x + y * (w + 1)] == 0 || bitmap_pixels[x + y * (w + 1)] == 15)
                continue;

            int s_x = x, s_y = y;
            int prevX = x, prevY = y;
            shapes.push_back({});
            shapeId++;
            do
            {
                // 0,15 do nothing
                switch (bitmap_pixels[s_x + s_y * (w + 1)])
                {
                case 1:
                case 3:
                case 7:
                    dx = 1;
                    dy = 0;
                    break;
                case 2:
                case 6:
                case 14:
                    dx = 0;
                    dy = -1;
                    break;
                case 4:
                case 12:
                case 13:
                    dx = -1;
                    dy = 0;
                    break;
                case 8:
                case 9:
                case 11:
                    dx = 0;
                    dy = 1;
                    break;
                case 5:
                    dy = 0;
                    if (prevY == s_y - 1)
                    {
                        dx = 1;
                        bitmap_pixels[s_x + s_y * (w + 1)] = 4; // left
                    }
                    else
                    {
                        dx = -1;
                        bitmap_pixels[s_x + s_y * (w + 1)] = 1; // right
                    }
                    prevX = s_x;
                    prevY = s_y;
                    s_x += dx;
                    s_y -= dy;
                    continue;
                case 10:
                    dx = 0;
                    if (prevX == s_x - 1)
                    {
                        dy = 1;
                        bitmap_pixels[s_x + s_y * (w + 1)] = 2; // down
                    }
                    else
                    {
                        dy = -1;
                        bitmap_pixels[s_x + s_y * (w + 1)] = 8; // up
                    }
                    prevX = s_x;
                    prevY = s_y;
                    s_x += dx;
                    s_y -= dy;
                    continue;
                }

                bitmap_pixels[s_x + s_y * (w + 1)] = 0;
                prevX = s_x;
                prevY = s_y;
                s_x += dx;
                s_y -= dy;
                shapes[shapeId].emplace_back(s_x, s_y);
            } while (x != s_x || y != s_y);
            if (threshold > 0 && shapes[shapeId].size() < threshold)
            {
                shapeId--;
                shapes.pop_back();
            }
        }
    }

    SDL_UnlockSurface(bitmap);
    SDL_FreeSurface(bitmap);

    return shapes;
}

double PointLineDistance(const b2Vec2 point, const b2Vec2 line1, const b2Vec2 line2)
{
    return std::abs((line2.x - line1.x) * (point.y - line1.y) -
                    (point.x - line1.x) * (line2.y - line1.y)) /
           sqrt(pow(line2.x - line1.x, 2) + pow(line2.y - line1.y, 2));
}

std::vector<b2Vec2> DouglasPeucker(std::span<b2Vec2> points, const double epsilon)
{
    double maxDistance = 0;
    int index = 0;
    for (int i = 1; i < points.size(); i++)
    {
        double dist = PointLineDistance(points[i], points[0], points[points.size() - 1]);
        if (dist > maxDistance)
        {
            index = i;
            maxDistance = dist;
        }
    }

    if (maxDistance > epsilon)
    {
        auto span1 = DouglasPeucker(points.subspan(0, index), epsilon);
        auto span2 = DouglasPeucker(points.subspan(index, points.size() - index), epsilon);

        std::vector<b2Vec2> newPoints{};
        newPoints.insert(newPoints.begin(), span1.begin(), span1.end());
        newPoints.insert(newPoints.end(), span2.begin(), span2.end());
        return newPoints;
    }
    else
        return std::vector<b2Vec2>{points[0], points[points.size() - 1]};
}