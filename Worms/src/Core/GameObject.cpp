#include "Core/GameObject.h"
#include "Game/GameScene.h"

GameScene& GameObject::Scene() const
{
    if (!scene) throw std::logic_error("GameObject has no scene");
    return *scene;
}

void GameObject::Initialise(SDL_Renderer* newRenderer, World* newWorld)
{
    if (!newWorld)
    {
        throw std::invalid_argument("GameObject requires a world");
    }
    if (hasEntity)
    {
        throw std::logic_error("GameObject already has an entity");
    }
    renderer = newRenderer;
    world = newWorld;
    objectId = world->CreateEntity();
    hasEntity = true;
    // Terminal::Get().Log(objectId + "", );
}

void GameObject::CleanUp()
{
    if (!hasEntity)
    {
        return;
    }
    world->DestroyEntity(objectId);
    hasEntity = false;
    objectId = {};
    world = nullptr;
    renderer = nullptr;
}
