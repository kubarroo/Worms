#pragma once
#include "Core/Camera/Camera.h"
#include "Core/Renderer2D.h"
#include "Game/Components.h"
#include <entt/entity/registry.hpp>
#include <numbers>

class Movement
{
public:
    static void Update(entt::registry& registry)
    {
        for (auto [entity, position, motion] : registry.view<Position, Motion>().each())
        {
            position.x += motion.v_x;
            position.y += motion.v_y;
        }
    }
};

class PhysicsSynchronizer
{
public:
    static void Update(entt::registry& registry)
    {
        for (auto [entity, position, rigidBody] : registry.view<Position, RigidBody>().each())
        {
            if (!rigidBody.body) continue;
            position.x = rigidBody.body->GetPosition().x;
            position.y = rigidBody.body->GetPosition().y;
            if (auto* angle = registry.try_get<Rotation>(entity))
                angle->degree = rigidBody.body->GetAngle() * 180 /
                                static_cast<float>(std::numbers::pi);
        }
    }
};

class TargetSystem
{
public:
    static void Update(entt::registry& registry)
    {
        for (auto [entity, position, follow] : registry.view<Position, Follow>().each())
        {
            if (!follow.id) continue;
            if (follow.id->owner != &registry || !registry.valid(follow.id->id))
            {
                follow.id.reset();
                continue;
            }
            const auto* target = registry.try_get<Position>(follow.id->id);
            if (!target)
            {
                follow.id.reset();
                continue;
            }
            const auto targetPosition = *target;
            position.x = targetPosition.x + follow.offsetX;
            position.y = targetPosition.y + follow.offsetY;
        }
    }
};

class ParticleUpdater
{
public:
    static void Update(entt::registry& registry)
    {
        for (auto [entity, particle, position, scale, sprite, motion] :
             registry.view<Particle, Position, Scale, Sprite, Motion>().each())
        {
            if (particle.vel_characteristic)
            {
                auto [x, y] = particle.vel_characteristic(motion.v_x, motion.v_y);
                motion.v_x = x;
                motion.v_y = y;
            }
            if (particle.size_characteristic)
                scale.size = particle.size_characteristic(scale.size);
        }
    }
};

class SpriteRenderer
{
public:
    static void Render(entt::registry& registry, Renderer2D& renderer, const Camera& camera)
    {
        registry.sort<RenderOrder>([](const RenderOrder& left, const RenderOrder& right)
                                  {
                                      if (left.layer != right.layer)
                                          return left.layer < right.layer;
                                      return left.order < right.order;
                                  });
        auto view = registry.view<const Position, const Sprite, const RenderOrder>();
        view.use<RenderOrder>();
        for (auto [entity, position, sprite, order] : view.each())
        {
            if (!sprite.texture) continue;
            const auto size = renderer.TextureSize(sprite.texture);
            const auto* scale = registry.try_get<Scale>(entity);
            const float factor = scale ? scale->size : 1.f;
            RenderRect destination;
            destination.x = static_cast<int>(400 + 100.0 * (position.x - camera.X()) -
                                             size.x / 2 * factor);
            destination.y = static_cast<int>(300 - 100.0 * (position.y - camera.Y()) -
                                             size.y / 2 * factor);
            destination.w = static_cast<int>(size.x * factor);
            destination.h = static_cast<int>(size.y * factor);
            const auto* angle = registry.try_get<Rotation>(entity);
            renderer.DrawSprite(sprite.texture, destination, {}, angle ? -angle->degree : 0.0);
        }
    }
};
