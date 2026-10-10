#include "SdlInputAdapter.h"
#include <SDL2/SDL.h>
#include <optional>

namespace
{
std::optional<InputAction> MapKey(SDL_Scancode key)
{
    switch (key)
    {
    case SDL_SCANCODE_A:
        return InputAction::Left;
    case SDL_SCANCODE_D:
        return InputAction::Right;
    case SDL_SCANCODE_W:
        return InputAction::AimUp;
    case SDL_SCANCODE_S:
        return InputAction::AimDown;
    case SDL_SCANCODE_LEFT:
        return InputAction::CameraLeft;
    case SDL_SCANCODE_RIGHT:
        return InputAction::CameraRight;
    case SDL_SCANCODE_UP:
        return InputAction::CameraUp;
    case SDL_SCANCODE_DOWN:
        return InputAction::CameraDown;
    case SDL_SCANCODE_LSHIFT:
        return InputAction::Fire;
    case SDL_SCANCODE_SPACE:
        return InputAction::Jump;
    case SDL_SCANCODE_E:
        return InputAction::NextWeapon;
    case SDL_SCANCODE_Q:
        return InputAction::PreviousWeapon;
    default:
        return {};
    }
}
} // namespace
void SdlInputAdapter::BeginFrame(bool keyboardCaptured) noexcept
{
    input.BeginFrame();
    input.SetEnabled(focused && !keyboardCaptured);
}
void SdlInputAdapter::ProcessEvent(const SDL_Event& event, bool keyboardCaptured) noexcept
{
    if (event.type == SDL_WINDOWEVENT)
    {
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
            focused = false;
        if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
            focused = true;
    }
    input.SetEnabled(focused && !keyboardCaptured);
    if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
        return;
    if (event.type == SDL_KEYDOWN && event.key.repeat)
        return;
    if (const auto action = MapKey(event.key.keysym.scancode))
        input.SetAction(*action, event.type == SDL_KEYDOWN);
}
void SdlInputAdapter::Reset() noexcept
{
    focused = true;
    input.Reset();
    input.SetEnabled(true);
}
