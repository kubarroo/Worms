#include "Input.h"

void Input::Reset() noexcept
{
    buttons = {};
    cameraControl = false;
    interrupted = false;
}
void Input::BeginFrame() noexcept
{
    interrupted = false;
    for (auto& button : buttons)
        button.pressed = button.released = button.consumed = false;
}
void Input::SetEnabled(bool value) noexcept
{
    if (enabled != value)
    {
        const bool wasInterrupted = interrupted;
        buttons = {};
        cameraControl = false;
        if (!value)
            ++interruptionCount;
        interrupted = wasInterrupted || !value;
    }
    enabled = value;
}
void Input::SetAction(InputAction action, bool down) noexcept
{
    const auto index = static_cast<std::size_t>(action);
    if (index >= buttons.size() || (!enabled && down))
        return;
    auto& button = buttons[index];
    if (button.held == down)
        return;
    button.held = down;
    if (down)
    {
        button.pressed = true;
        if (action <= InputAction::AimDown)
            cameraControl = false;
        else if (action <= InputAction::CameraDown)
            cameraControl = true;
    }
    else
        button.released = true;
}
bool Input::Held(InputAction action) const noexcept
{
    const auto index = static_cast<std::size_t>(action);
    return index < buttons.size() && buttons[index].held;
}
bool Input::Pressed(InputAction action) const noexcept
{
    const auto index = static_cast<std::size_t>(action);
    return index < buttons.size() && buttons[index].pressed;
}
bool Input::Released(InputAction action) const noexcept
{
    const auto index = static_cast<std::size_t>(action);
    return index < buttons.size() && buttons[index].released;
}
bool Input::ConsumePress(InputAction action) noexcept
{
    const auto index = static_cast<std::size_t>(action);
    if (index >= buttons.size())
        return false;
    auto& button = buttons[index];
    if (!button.pressed || button.consumed)
        return false;
    button.consumed = true;
    return true;
}
