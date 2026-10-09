#pragma once
#include "Core/Camera/Camera.h"
#include "ECS/System.h"
#include "Game/Components.h"

class Movement : public System
{
public:
    Movement(ComponentManager& componentManager) : System(componentManager)
    {
        systemSignature.set(componentManager.GetComponentId<Position>(), true);
        systemSignature.set(componentManager.GetComponentId<Motion>(), true);
    }

    virtual void Update() override
    {
        auto& positions = componentManager.GetComponentArray<Position>();
        auto& motions = componentManager.GetComponentArray<Motion>();
        for (EntityId ent : subscribed)
        {
            positions.GetData(ent).x += motions.GetData(ent).v_x;
            positions.GetData(ent).y += motions.GetData(ent).v_y;
        }
    }
};

class SpriteRenderer : public System
{
public:
    SpriteRenderer(ComponentManager& componentManager, SDL_Renderer* renderer, Camera& camera)
        : System(componentManager), renderer(renderer), camera(camera)
    {
        systemSignature.set(componentManager.GetComponentId<Position>(), true);
        systemSignature.set(componentManager.GetComponentId<Sprite>(), true);
    }

    virtual void Render() override
    {
        auto& positions = componentManager.GetComponentArray<Position>();
        auto& sprites = componentManager.GetComponentArray<Sprite>();
        for (EntityId ent : subscribed)
        {
            if (!sprites.GetData(ent).texture)
            {
                continue;
            }
            SDL_Point size;
            SDL_QueryTexture(sprites.GetData(ent).texture, NULL, NULL, &size.x, &size.y);

            auto scale = componentManager.TryGetComponent<Scale>(ent);
            float scaleFactor = 1.f;
            if (scale.has_value())
                scaleFactor = scale->get().size;

            SDL_Rect destination;
            destination.x = static_cast<int>(400 + 100.0 * (positions.GetData(ent).x - camera.X()) -
                                             size.x / 2 * scaleFactor);
            destination.y = static_cast<int>(300 - 100.0 * (positions.GetData(ent).y - camera.Y()) -
                                             size.y / 2 * scaleFactor);
            destination.w = static_cast<int>(size.x * scaleFactor);
            destination.h = static_cast<int>(size.y * scaleFactor);

            auto angle = componentManager.TryGetComponent<Rotation>(ent);
            if (angle.has_value())
                SDL_RenderCopyEx(renderer, sprites.GetData(ent).texture, NULL, &destination,
                                 -angle.value().get().degree, NULL,
                                 SDL_RendererFlip::SDL_FLIP_NONE);
            else
                SDL_RenderCopy(renderer, sprites.GetData(ent).texture, NULL, &destination);
        }
    }

private:
    SDL_Renderer* renderer;
    Camera& camera;
};

class PhysicsSynchronizer : public System
{
public:
    PhysicsSynchronizer(ComponentManager& componentManager) : System(componentManager)
    {
        systemSignature.set(componentManager.GetComponentId<Position>(), true);
        systemSignature.set(componentManager.GetComponentId<RigidBody>(), true);
    }

    virtual void Update() override
    {
        auto& positions = componentManager.GetComponentArray<Position>();
        auto& rigidBody = componentManager.GetComponentArray<RigidBody>();
        for (EntityId ent : subscribed)
        {
            if (!rigidBody.GetData(ent).body)
            {
                continue;
            }
            positions.GetData(ent).x = rigidBody.GetData(ent).body->GetPosition().x;
            positions.GetData(ent).y = rigidBody.GetData(ent).body->GetPosition().y;

            auto angle = componentManager.TryGetComponent<Rotation>(ent);
            if (angle.has_value())
                angle.value().get().degree =
                    rigidBody.GetData(ent).body->GetAngle() * 180 / static_cast<float>(M_PI);
        }
    }

private:
};

class TargetSystem : public System
{
public:
    TargetSystem(ComponentManager& componentManager, World& world)
        : System(componentManager), world(world)
    {
        systemSignature.set(componentManager.GetComponentId<Follow>(), true);
        systemSignature.set(componentManager.GetComponentId<Position>(), true);
    }

    virtual void Update() override
    {
        auto& positions = componentManager.GetComponentArray<Position>();
        auto& follows = componentManager.GetComponentArray<Follow>();
        for (EntityId ent : subscribed)
        {
            auto& target = follows.GetData(ent);
            if (!target.id)
            {
                continue;
            }
            if (!world.IsAlive(*target.id))
            {
                target.id.reset();
                continue;
            }
            auto targetComponent = positions.TryGetData(target.id->id);
            if (!targetComponent)
            {
                target.id.reset();
                continue;
            }
            const auto targetPosition = targetComponent->get();
            positions.GetData(ent).x = targetPosition.x + target.offsetX;
            positions.GetData(ent).y = targetPosition.y + target.offsetY;
        }
    }

private:
    World& world;
};

class ParticleUpdater : public System
{
public:
    ParticleUpdater(ComponentManager& componentManager) : System(componentManager)
    {
        systemSignature.set(componentManager.GetComponentId<Particle>(), true);
        systemSignature.set(componentManager.GetComponentId<Position>(), true);
        systemSignature.set(componentManager.GetComponentId<Scale>(), true);
        systemSignature.set(componentManager.GetComponentId<Sprite>(), true);
        systemSignature.set(componentManager.GetComponentId<Motion>(), true);
    }

    void Update() override
    {
        auto& particles = componentManager.GetComponentArray<Particle>();
        auto& positions = componentManager.GetComponentArray<Position>();
        auto& scales = componentManager.GetComponentArray<Scale>();
        auto& sprites = componentManager.GetComponentArray<Sprite>();
        auto& motions = componentManager.GetComponentArray<Motion>();

        for (EntityId ent : subscribed)
        {
            auto& particle = particles.GetData(ent);
            auto& motion = motions.GetData(ent);
            if (particle.vel_characteristic != nullptr)
            {
                auto [x, y] = particle.vel_characteristic(motion.v_x, motion.v_y);
                motion.v_x = x;
                motion.v_y = y;
            }

            if (particle.size_characteristic != nullptr)
                scales.GetData(ent).size = particle.size_characteristic(scales.GetData(ent).size);
        }
    }
};
