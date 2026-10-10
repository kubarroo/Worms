#pragma once
#include "ECS/ECS_Types.h"
#include <SDL2/SDL.h>
#include <box2d/b2_body.h>
#include <functional>
#include <optional>

struct Position
{
    float x = 0.f, y = 0.f;
};

struct Motion
{
    float v_x = 0.f, v_y = 0.f;
};

struct Scale
{
    float size = 1.f;
};

struct Sprite
{
    // Borrowed; the texture owner must outlive this component.
    SDL_Texture* texture = nullptr;
};

struct RigidBody
{
    b2Body* body = nullptr;
};

struct Rotation
{
    float degree;
};

struct Health
{
    int current;
    int max;
};

struct Follow
{
    std::optional<EntityHandle> id;
    float offsetX = 0.f;
    float offsetY = 0.f;
};

struct Particle
{
    float* progress = nullptr;
    std::function<std::pair<float, float>(float, float)> pos_characteristic = nullptr;
    std::function<std::pair<float, float>(float, float)> vel_characteristic = nullptr;
    std::function<float(float)> size_characteristic = nullptr;
};
