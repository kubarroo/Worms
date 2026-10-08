#pragma once
#include "ECS/ECS_Types.h"
#include "ECS/World.h"
#include "SDL2/SDL.h"
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
        return objectId;
    }

    static std::vector<std::unique_ptr<GameObject>> activeObjs;
    static std::vector<std::unique_ptr<GameObject>> objsToAdd;
    static std::vector<GameObject*> objsToDelete;

protected:
    EntityId objectId;
    World* world = nullptr;
    SDL_Renderer* renderer = nullptr;
};
