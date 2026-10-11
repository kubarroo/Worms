#pragma once
#include "ECS/ECS_Types.h"
#include <entt/entity/registry.hpp>
#include <functional>
#include <optional>
#include <stdexcept>
#include <utility>

// Temporary gameplay adapter. The scene owns the registry and must outlive this object.
class World
{
public:
    explicit World(entt::registry& registry) : registry(registry) {}
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    entt::registry& Registry() { return registry; }
    const entt::registry& Registry() const { return registry; }
    EntityId CreateEntity() { return registry.create(); }
    void DestroyEntity(EntityId entity)
    {
        if (IsAlive(entity)) registry.destroy(entity);
    }
    bool IsAlive(EntityId entity) const { return registry.valid(entity); }
    bool IsAlive(EntityHandle handle) const
    {
        return handle.owner == &registry && IsAlive(handle.id);
    }
    std::optional<EntityHandle> GetHandle(EntityId entity) const
    {
        if (!IsAlive(entity)) return {};
        return EntityHandle{entity, &registry};
    }
    std::size_t GetEntityCount() const
    {
        return std::as_const(registry).storage<EntityId>()->free_list();
    }

    template <typename Component> Component& AddComponent(EntityId entity)
    {
        RequireNewComponent<Component>(entity);
        return registry.emplace<Component>(entity);
    }
    template <typename Component> Component& AddComponent(EntityId entity, Component component)
    {
        RequireNewComponent<Component>(entity);
        return registry.emplace<Component>(entity, std::move(component));
    }
    template <typename Component> Component& GetComponent(EntityId entity)
    {
        auto result = TryGetComponent<Component>(entity);
        if (!result) throw std::logic_error("Entity has no requested component");
        return result->get();
    }
    template <typename Component>
    std::optional<std::reference_wrapper<Component>> TryGetComponent(EntityId entity)
    {
        if (!IsAlive(entity)) return {};
        if (auto* component = registry.try_get<Component>(entity)) return std::ref(*component);
        return {};
    }
    template <typename Component> void RemoveComponent(EntityId entity)
    {
        RequireAlive(entity);
        registry.remove<Component>(entity);
    }

private:
    void RequireAlive(EntityId entity) const
    {
        if (!IsAlive(entity)) throw std::logic_error("Entity is not alive");
    }
    template <typename Component> void RequireNewComponent(EntityId entity) const
    {
        RequireAlive(entity);
        if (registry.all_of<Component>(entity))
            throw std::logic_error("Entity already has the component");
    }
    entt::registry& registry;
};
