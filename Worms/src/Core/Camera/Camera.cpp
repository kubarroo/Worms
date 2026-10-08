#include "Core/Camera/Camera.h"
#include "Core/Input.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"

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

void Camera::Initialise(SDL_Renderer* newRenderer, World* newWorld)
{
    GameObject::Initialise(newRenderer, newWorld);
    pos = &world->AddComponent<Position>(objectId, {2, -1});
    focusPoint = std::make_unique<FocusPoint>(newRenderer, newWorld);
}

void Camera::Update()
{
    if (Input::Get().CameraControll())
    {
        ChangeX(Input::Get().CameraHorizontal() * static_cast<float>(Time::deltaTime) *
                CAMERA_SPEED);
        ChangeY(Input::Get().CameraVertical() * static_cast<float>(Time::deltaTime) * CAMERA_SPEED);
        return;
    }
    if (focusPoint->GetPos().has_value())
    {
        ChangePos(adjustPos(focusPoint->GetPos().value(), *pos));
        timer.Reset();
    }
    else if (timer.Measure() > 1.5)
    {
        noTargetEvent();
    }
}