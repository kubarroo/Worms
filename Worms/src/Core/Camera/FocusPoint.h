#pragma once

#include "Core/GameObject.h"
#include "Game/Components.h"
#include "Terminal/Terminal.h"
#include <optional>

class FocusPoint : public GameObject
{
public:
    FocusPoint(SDL_Renderer* newRenderer, World* newWorld);
    void CleanUp() override
    {
        ClearTarget();
        GameObject::CleanUp();
    }
    void ChangeTarget(EntityId newTargetId)
    {
        target = HasEntity() ? world->GetHandle(newTargetId) : std::nullopt;
    }
    void ClearTarget()
    {
        target.reset();
    }
    std::optional<Position> GetPos()
    {
        if (!HasEntity() || !target)
        {
            return {};
        }
        if (!world->IsAlive(*target))
        {
            ClearTarget();
            return {};
        }
        auto position = world->TryGetComponent<Position>(target->id);
        if (!position)
        {
            ClearTarget();
            return {};
        }
        return position->get();
    };

private:
    std::optional<EntityHandle> target;
};
