#pragma once
#include "Core/EntityHandle.h"
#include <SDL3/SDL.h>
#include <box2d/b2_body.h>
#include <functional>
#include <optional>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

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
    // Borrowed from Box2D; the object owner destroys the body before its entity.
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

template <typename Signature> class ParticleCharacteristic;

template <typename Result, typename... Args>
class ParticleCharacteristic<Result(Args...)>
{
public:
    ParticleCharacteristic() = default;
    ParticleCharacteristic(std::nullptr_t) {}

    template <typename Callable>
        requires (!std::is_same_v<std::remove_cvref_t<Callable>, ParticleCharacteristic> &&
                  std::is_constructible_v<std::function<Result(Args...)>, Callable>)
    ParticleCharacteristic(Callable&& callable)
    {
        std::function<Result(Args...)> value(std::forward<Callable>(callable));
        if (value) function = std::make_shared<std::function<Result(Args...)>>(std::move(value));
    }

    explicit operator bool() const noexcept { return static_cast<bool>(function); }
    Result operator()(Args... args) const
    {
        if (!function) throw std::bad_function_call();
        // Hold the callable even if it removes or replaces its own component.
        auto invocation = function;
        return (*invocation)(std::forward<Args>(args)...);
    }

private:
    std::shared_ptr<std::function<Result(Args...)>> function;
};

struct Particle
{
    // Borrowed from the non-movable ParticleSystem, which destroys its particles first.
    float* progress = nullptr;
    // Copies retain the same callable state and keep an executing callback alive.
    ParticleCharacteristic<std::pair<float, float>(float, float)> pos_characteristic = nullptr;
    ParticleCharacteristic<std::pair<float, float>(float, float)> vel_characteristic = nullptr;
    ParticleCharacteristic<float(float)> size_characteristic = nullptr;
};
