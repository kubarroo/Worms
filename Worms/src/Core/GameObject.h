#pragma once
#include "Core/SceneContext.h"
#include "ECS/ECS_Types.h"
#include "ECS/World.h"
#include "SDL2/SDL.h"
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

    virtual ~GameObject() = default;

protected:
    const SceneContext& Context() const;
    EntityId objectId{};
    bool hasEntity = false;
    World* world = nullptr;
    SDL_Renderer* renderer = nullptr;

private:
    friend class GameScene;
    std::optional<SceneContext> context;
};
