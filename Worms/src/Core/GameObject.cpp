#include "Core/GameObject.h"
#include "Core/Physics/ContactManager.h"
#include <box2d/b2_world.h>

void GameObject::RequireUnlockedPhysics() const
{
    if (context && context->physics.IsLocked())
        throw std::logic_error("Object cleanup requires an unlocked physics world");
}

const SceneContext& GameObject::Context() const
{
    if (!context)
        throw std::logic_error("GameObject is not initialized");
    return *context;
}

void GameObject::Initialise(const SceneContext& newContext)
{
    if (newContext.physics.IsLocked())
        throw std::logic_error("Object initialization requires an unlocked physics world");
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
    RequireUnlockedPhysics();
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
