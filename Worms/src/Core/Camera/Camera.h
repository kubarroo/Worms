#pragma once
#include "Core/Camera/FocusPoint.h"
#include "Core/Time.h"
#include <algorithm>
#include <memory>

class Camera : public GameObject
{
public:
    float X() const
    {
        return GetPosition().x;
    };
    float Y() const
    {
        return GetPosition().y;
    };
    float Zoom()
    {
        return zoom;
    };

    void Initialise(SDL_Renderer* newRenderer, World* newWorld) override;
    void Update() override;
    void CleanUp() override;
    void ChangePos(Position newPos)
    {
        auto& pos = GetPosition();
        pos.x += newPos.x;
        pos.y += newPos.y;
    }
    void ChangeX(float deltaX)
    {
        GetPosition().x += deltaX;
    }
    void ChangeY(float deltaY)
    {
        GetPosition().y += deltaY;
    }
    void ChangeZoom(float delta)
    {
        zoom += delta;
    }
    void ChangeTarget(EntityId newTargetId)
    {
        if (!focusPoint)
        {
            throw std::logic_error("Camera has no focus point");
        }
        focusPoint->ChangeTarget(newTargetId);
    }
    void ClearTarget()
    {
        if (focusPoint)
        {
            focusPoint->ClearTarget();
        }
    }

    std::function<void()> noTargetEvent = nullptr;

private:
    float zoom = 1.f;
    bool inputs_enabled = false;
    Position& GetPosition() const;
    std::unique_ptr<FocusPoint> focusPoint;
    Time::Timer timer{};

    static constexpr float CAMERA_SPEED = 2.f;
};
