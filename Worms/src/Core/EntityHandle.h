#pragma once
#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>
#include <entt/entity/registry.hpp>

using EntityId = entt::entity;

// EnTT versions reject recycled entities; the owner rejects foreign registries.
// The original registry must still be alive when this handle is checked.
struct EntityHandle
{
    EntityId id = entt::null;
    const entt::registry* owner = nullptr;
    bool operator==(const EntityHandle&) const = default;
};

inline bool IsValid(const entt::registry& registry, EntityHandle handle)
{
    return handle.owner == &registry && registry.valid(handle.id);
}
