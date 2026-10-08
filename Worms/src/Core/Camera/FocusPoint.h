#pragma once

#include "Core/GameObject.h"
#include "Game/Components.h"
#include "Terminal/Terminal.h"
#include <optional>

class FocusPoint : public GameObject
{
public:
    FocusPoint(SDL_Renderer* newRenderer, World* newWorld);
    void ChangeTarget(EntityId newTargetId)
    {
        target = newTargetId;
    }
    std::optional<std::reference_wrapper<Position>> GetPos() const
    {
        return world->TryGetComponent<Position>(target);
    };

private:
    EntityId target;
};