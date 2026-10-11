#pragma once

#include "Core/GameObject.h"
#include "Game/Components.h"
#include "Terminal/Terminal.h"
#include <optional>

class FocusPoint : public GameObject
{
public:
    FocusPoint() = default;
    void CleanUp() override
    {
        ClearTarget();
        GameObject::CleanUp();
    }
    void ChangeTarget(EntityHandle newTarget)
    {
        if (!HasEntity())
        {
            ClearTarget();
            return;
        }
        if (newTarget.owner != &Context().registry)
            throw std::invalid_argument("Camera target belongs to another registry");
        target = world->IsAlive(newTarget) ? std::optional{newTarget} : std::nullopt;
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
