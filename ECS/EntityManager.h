#pragma once
#include "ECS/ECS_Types.h"
#include <array>
#include <bitset>
#include <queue>
#include <optional>


class EntityManager
{
public:
    EntityManager();
    EntityManager(const EntityManager&) = delete;
    EntityManager& operator=(const EntityManager&) = delete;
    EntityManager(EntityManager&&) = delete;
    EntityManager& operator=(EntityManager&&) = delete;

    EntityId CreateEntity();
    void DestroyEntity(const EntityId ent);
    bool IsAlive(EntityId ent) const;
    bool IsAlive(EntityHandle handle) const;
    std::optional<EntityHandle> GetHandle(EntityId ent) const;

    Signature AddToSignature(const EntityId ent, const ComponentType type);
    Signature DeleteFromSignature(const EntityId ent, const ComponentType type);
    void SetSignature(const EntityId ent, const Signature signature);
    Signature GetSignature(const EntityId ent);

    uint16_t GetAmountOfAvailableEntities() const;

private:
    std::queue<EntityId> availableEntities;
    void RequireAlive(EntityId ent) const;
    std::array<Signature, MAX_ENTITIES> signatures{};
    std::array<uint64_t, MAX_ENTITIES> generations{};
    std::bitset<MAX_ENTITIES> alive;
};
