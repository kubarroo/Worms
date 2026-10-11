#pragma once
#include "Core/Camera/Camera.h"
#include "Core/Renderer2D.h"
#include "Game/Components.h"
#include <entt/entity/registry.hpp>
#include <numbers>
#include <vector>

class Movement
{
public:
    static void Update(entt::registry& registry)
    {
        for (auto [entity, position, motion] : registry.view<Position, const Motion>().each())
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
        for (auto [entity, position, rigidBody] : registry.view<Position, const RigidBody>().each())
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
            const auto* target = registry.try_get<const Position>(follow.id->id);
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
        // Callbacks may compact pools. Never keep iterators or component references across them.
        std::vector<EntityId> entities;
        for (auto entity : registry.view<const Particle, const Position, Scale, const Sprite, Motion>())
            entities.push_back(entity);
        const auto eligible = [&registry](EntityId entity)
        {
            return registry.valid(entity) &&
                registry.all_of<Particle, Position, Scale, Sprite, Motion>(entity);
        };
        for (auto entity : entities)
        {
            if (!eligible(entity)) continue;
            const auto particle = registry.get<Particle>(entity);
            if (particle.vel_characteristic)
            {
                const auto motion = registry.get<Motion>(entity);
                auto [x, y] = particle.vel_characteristic(motion.v_x, motion.v_y);
                if (!eligible(entity)) continue;
                registry.get<Motion>(entity) = Motion{x, y};
            }
            if (particle.size_characteristic)
            {
                const auto size = registry.get<Scale>(entity).size;
                const auto result = particle.size_characteristic(size);
                if (eligible(entity)) registry.get<Scale>(entity).size = result;
            }
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
            const auto* scale = registry.try_get<const Scale>(entity);
            const float factor = scale ? scale->size : 1.f;
            RenderRect destination;
            destination.x = static_cast<int>(400 + 100.0 * (position.x - camera.X()) -
                                             size.x / 2 * factor);
            destination.y = static_cast<int>(300 - 100.0 * (position.y - camera.Y()) -
                                             size.y / 2 * factor);
            destination.w = static_cast<int>(size.x * factor);
            destination.h = static_cast<int>(size.y * factor);
            const auto* angle = registry.try_get<const Rotation>(entity);
            renderer.DrawSprite(sprite.texture, destination, {}, angle ? -angle->degree : 0.0);
        }
    }
};
