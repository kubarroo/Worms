#pragma once
#include "ECS/ECS_Types.h"
#include "ECS/World.h"
#include "SDL2/SDL.h"
#include <stdexcept>
#include <vector>


class GameObject
{
public:
    virtual void Initialise(SDL_Renderer* newRenderer, World* newWorld);
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

    static std::vector<std::unique_ptr<GameObject>> activeObjs;
    static std::vector<std::unique_ptr<GameObject>> objsToAdd;
    static std::vector<GameObject*> objsToDelete;

    virtual ~GameObject() = default;

protected:
    EntityId objectId{};
    bool hasEntity = false;
    World* world = nullptr;
    SDL_Renderer* renderer = nullptr;
};
