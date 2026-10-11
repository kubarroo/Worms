#pragma once
#include "ECS/ECS_Types.h"
#include <SDL3/SDL.h>
#include <box2d/b2_body.h>
#include <functional>
#include <optional>
#include <cstdint>

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

enum class RenderLayer
{
    Terrain,
    Actors,
    Weapons,
    Projectiles,
    Effects
};

struct RenderOrder
{
    RenderLayer layer = RenderLayer::Actors;
    std::uint64_t order = 0;
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
