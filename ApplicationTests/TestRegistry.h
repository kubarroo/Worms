#pragma once
#include <entt/entity/registry.hpp>
#include <stdexcept>
#include "Core/EntityHandle.h"
#include <optional>
#include <iterator>

inline std::size_t LiveEntityCount(const entt::registry& registry)
{
    const auto entities = registry.view<entt::entity>();
    return static_cast<std::size_t>(std::distance(entities.begin(), entities.end()));
}

inline std::optional<EntityHandle> MakeHandle(const entt::registry& registry, entt::entity entity)
{
    if (!registry.valid(entity)) return {};
    return EntityHandle{entity, &registry};
}

// Inject a failure after insertion, exercising cleanup of partially initialized objects.
template <typename Component> class ScopedComponentFailure
{
public:
    explicit ScopedComponentFailure(entt::registry& registry)
        : connection(registry.on_construct<Component>()
                         .template connect<&ScopedComponentFailure::Fail>(*this)) {}
    ScopedComponentFailure(const ScopedComponentFailure&) = delete;
    ScopedComponentFailure& operator=(const ScopedComponentFailure&) = delete;
    void Stop() { connection.release(); }

private:
    void Fail(entt::registry&, entt::entity)
    {
        throw std::runtime_error("Injected component construction failure");
    }
    entt::scoped_connection connection;
};
