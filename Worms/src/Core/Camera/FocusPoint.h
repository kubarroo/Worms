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
        target = newTargetId;
    }
    void ClearTarget()
    {
        target.reset();
    }
    std::optional<std::reference_wrapper<Position>> GetPos() const
    {
        if (!HasEntity() || !target)
        {
            return {};
        }
        return world->TryGetComponent<Position>(*target);
    };

private:
    std::optional<EntityId> target;
};
