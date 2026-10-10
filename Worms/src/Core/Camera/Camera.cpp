#include "Core/Camera/Camera.h"
#include "Core/Input.h"
#include "Core/Audio/Audio.h"
#include "Core/Renderer2D.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <utility>

double smoothstep(double x, double endPoint, double currentPoint)
{
    int noTraceBarrier = 1.;
    int maxTace = 5.0;
    int sign = (endPoint - currentPoint > 0) ? 1 : -1;
    if (fabs(x) < 1.0)
        return 0;
    x = std::clamp((fabs(x) - noTraceBarrier) / (maxTace - noTraceBarrier), 0.0, 0.2);
    return x * x * (3 - 2 * x) * sign;
}

Position adjustPos(const Position& focusPos, const Position& currentPos)
{
    Position newPos{0, 0};
    double dx = focusPos.x - currentPos.x;
    double dy = focusPos.y - 0.5f - currentPos.y;
    newPos.x = smoothstep(dx, focusPos.x, currentPos.x);
    newPos.y = smoothstep(dy, focusPos.y - 0.5f, currentPos.y);
    return newPos;
}

void Camera::Initialise(const SceneContext& context)
{
    GameObject::Initialise(context);
    try
    {
        world->AddComponent<Position>(objectId, {2, -1});
        focusPoint = std::make_unique<FocusPoint>();
        focusPoint->Initialise(context);
        timer.Reset();
        targetLostPending = targetLossReported = false;
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}

void Camera::Update()
{
    if (!HasEntity() || !focusPoint)
    {
        return;
    }
    if (Context().input.CameraControll())
    {
        ChangeX(Context().input.CameraHorizontal() * static_cast<float>(Time::deltaTime) *
                CAMERA_SPEED);
        ChangeY(Context().input.CameraVertical() * static_cast<float>(Time::deltaTime) * CAMERA_SPEED);
        return;
    }
    auto targetPosition = focusPoint->GetPos();
    if (targetPosition.has_value())
    {
        targetLostPending = targetLossReported = false;
        ChangePos(adjustPos(*targetPosition, GetPosition()));
        timer.Reset();
    }
    else if (timer.Measure() > 1.5 && !targetLossReported)
    {
        targetLostPending = true;
        targetLossReported = true;
    }
}

bool Camera::ConsumeTargetLost() noexcept
{
    return std::exchange(targetLostPending, false);
}

Position& Camera::GetPosition() const
{
    if (!HasEntity())
    {
        throw std::logic_error("Camera has no entity");
    }
    return world->GetComponent<Position>(objectId);
}

void Camera::CleanUp()
{
    targetLostPending = targetLossReported = false;
    if (focusPoint)
    {
        focusPoint->CleanUp();
        focusPoint.reset();
    }
    GameObject::CleanUp();
}
