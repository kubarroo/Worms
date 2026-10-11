#pragma once
#include <entt/entity/registry.hpp>
#include <stdexcept>

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
