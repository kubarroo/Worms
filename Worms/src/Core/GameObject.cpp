#include "Core/GameObject.h"
#include "Terminal/Terminal.h"

std::vector<std::unique_ptr<GameObject>> GameObject::activeObjs;
std::vector<std::unique_ptr<GameObject>> GameObject::objsToAdd;
std::vector<GameObject*> GameObject::objsToDelete;

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
