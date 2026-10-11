#include "Core/GameObject.h"
#include "Core/Physics/ContactManager.h"

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
    if (&newContext.contacts.Registry() != &newContext.registry)
        throw std::invalid_argument("Contact manager uses a different ECS registry");
    const auto newId = newContext.registry.create();
    context.emplace(newContext);
    renderer = newContext.renderer;
    objectId = newId;
    hasEntity = true;
}

void GameObject::CleanUp()
{
    if (!hasEntity)
    {
        return;
    }
    auto& registry = Registry();
    if (registry.valid(objectId)) registry.destroy(objectId);
    hasEntity = false;
    objectId = entt::null;
    renderer = nullptr;
    context.reset();
}
