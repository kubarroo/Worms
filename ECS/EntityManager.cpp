#include "ECS/EntityManager.h"
#include <stdexcept>

EntityManager::EntityManager()
{
    for (EntityId e = 0u; e < MAX_ENTITIES; e++)
        availableEntities.emplace(e);
}

EntityId EntityManager::CreateEntity()
{
    if (availableEntities.empty())
        throw std::runtime_error("Entity capacity exhausted");
    EntityId newEntity = availableEntities.front();
    LOG("Created entity " + std::to_string(newEntity));
    availableEntities.pop();
    alive.set(newEntity);
    return newEntity;
}

void EntityManager::DestroyEntity(const EntityId ent)
{
    if (!IsAlive(ent)) return;
    LOG("Destroyed entity " + std::to_string(ent));
    availableEntities.push(ent);
    alive.reset(ent);
    signatures[ent].reset();
    ++generations[ent];
}

bool EntityManager::IsAlive(EntityId ent) const
{
    return ent < MAX_ENTITIES && alive.test(ent);
}

bool EntityManager::IsAlive(EntityHandle handle) const
{
    return handle.owner == this && IsAlive(handle.id) &&
           generations[handle.id] == handle.generation;
}

std::optional<EntityHandle> EntityManager::GetHandle(EntityId ent) const
{
    if (!IsAlive(ent)) return {};
    return EntityHandle{ent, generations[ent], this};
}

void EntityManager::RequireAlive(EntityId ent) const
{
    if (!IsAlive(ent)) throw std::logic_error("Entity is not alive");
}

Signature EntityManager::AddToSignature(const EntityId ent, const ComponentType type)
{
    RequireAlive(ent);
    return signatures[ent].set(type, true);
}

Signature EntityManager::DeleteFromSignature(const EntityId ent, const ComponentType type)
{
    RequireAlive(ent);
    return signatures[ent].set(type, false);
}

void EntityManager::SetSignature(const EntityId ent, const Signature signature)
{
    RequireAlive(ent);
    signatures[ent] = signature;
}

Signature EntityManager::GetSignature(const EntityId ent)
{
    RequireAlive(ent);
    return signatures[ent];
}

uint16_t EntityManager::GetAmountOfAvailableEntities() const
{
    return static_cast<uint16_t>(availableEntities.size());
}
