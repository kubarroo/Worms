#include "Core/GameObject.h"

const SceneContext& GameObject::Context() const
{
    if (!context)
        throw std::logic_error("GameObject is not initialized");
    return *context;
}

void GameObject::Initialise(const SceneContext& newContext)
{
    if (hasEntity)
    {
        throw std::logic_error("GameObject already has an entity");
    }
    if (&newContext.registry != &newContext.world.Registry())
        throw std::invalid_argument("Context uses different ECS registries");
    const auto newId = newContext.world.CreateEntity();
    context.emplace(newContext);
    renderer = newContext.renderer;
    world = &newContext.world;
    objectId = newId;
    hasEntity = true;
}

void GameObject::CleanUp()
{
    if (!hasEntity)
    {
        return;
    }
    world->DestroyEntity(objectId);
    hasEntity = false;
    objectId = entt::null;
    world = nullptr;
    renderer = nullptr;
    context.reset();
}
