#pragma once
#include "Game/Components.h"
#include <entt/entity/registry.hpp>

struct RenderSequence
{
    std::uint64_t next = 0;
};

inline RenderOrder NextRenderOrder(entt::registry& registry, RenderLayer layer)
{
    auto* sequence = registry.ctx().find<RenderSequence>();
    if (!sequence)
        sequence = &registry.ctx().emplace<RenderSequence>();
    return {layer, sequence->next++};
}
