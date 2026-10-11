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
        target = IsValid(Registry(), newTarget) ? std::optional{newTarget} : std::nullopt;
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
        if (!IsValid(Registry(), *target))
        {
            ClearTarget();
            return {};
        }
        auto position = Registry().try_get<Position>(target->id);
        if (!position)
        {
            ClearTarget();
            return {};
        }
        return *position;
    };

private:
    std::optional<EntityHandle> target;
};
