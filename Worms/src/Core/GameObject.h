#pragma once
#include "Core/SceneContext.h"
#include "ECS/ECS_Types.h"
#include "SDL3/SDL.h"
#include <optional>
#include <stdexcept>

class GameObject
{
public:
    GameObject() = default;
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = delete;
    GameObject& operator=(GameObject&&) = delete;

    virtual void Initialise(const SceneContext& context);
    virtual void Update() {};
    virtual void Render() {};
    virtual void CleanUp();

    EntityId GetId() const
    {
        if (!hasEntity)
        {
            throw std::logic_error("GameObject has no entity");
        }
        return objectId;
    }

    bool HasEntity() const
    {
        return hasEntity;
    }

    EntityHandle GetHandle() const
    {
        const auto id = GetId();
        auto& registry = Registry();
        if (!registry.valid(id))
            throw std::logic_error("GameObject entity is no longer alive");
        return {id, &registry};
    }

    virtual ~GameObject() = default;

protected:
    const SceneContext& Context() const;
    entt::registry& Registry() const { return Context().registry; }
    EntityId objectId = entt::null;
    bool hasEntity = false;
    SDL_Renderer* renderer = nullptr;

private:
    friend class GameScene;
    std::optional<SceneContext> context;
};
