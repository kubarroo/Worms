#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

enum class InputAction
{
    Left,
    Right,
    AimUp,
    AimDown,
    CameraLeft,
    CameraRight,
    CameraUp,
    CameraDown,
    Fire,
    Jump,
    NextWeapon,
    PreviousWeapon,
    ChangeWorm,
    ChangeTeam,
    Count
};

// Per-application action state; independent of platform events.
class Input
{
public:
    Input() = default;
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;
    void BeginFrame() noexcept;
    void SetAction(InputAction action, bool down) noexcept;
    void SetEnabled(bool value) noexcept;
    bool Enabled() const noexcept
    {
        return enabled;
    }
    bool Interrupted() const noexcept
    {
        return interrupted;
    }
    std::uint64_t InterruptionCount() const noexcept
    {
        return interruptionCount;
    }
    bool Held(InputAction action) const noexcept;
    bool Pressed(InputAction action) const noexcept;
    bool Released(InputAction action) const noexcept;
    bool ConsumePress(InputAction action) noexcept;
    void Reset() noexcept;
    float Horizontal() const
    {
        return Axis(InputAction::Left, InputAction::Right);
    }
    float Vertical() const
    {
        return Axis(InputAction::AimDown, InputAction::AimUp);
    }
    float CameraHorizontal() const
    {
        return Axis(InputAction::CameraLeft, InputAction::CameraRight);
    }
    float CameraVertical() const
    {
        return Axis(InputAction::CameraDown, InputAction::CameraUp);
    }
    bool ChangeWorm()
    {
        return ConsumePress(InputAction::ChangeWorm);
    }
    bool ChangeTeam()
    {
        return ConsumePress(InputAction::ChangeTeam);
    }
    bool UseAction() const
    {
        return Held(InputAction::Fire);
    }
    bool Jump() const
    {
        return Held(InputAction::Jump);
    }
    int ChangeWeapon()
    {
        return static_cast<int>(ConsumePress(InputAction::NextWeapon)) -
               static_cast<int>(ConsumePress(InputAction::PreviousWeapon));
    }
    bool CameraControll() const
    {
        return cameraControl;
    }

private:
    struct Button
    {
        bool held = false, pressed = false, released = false, consumed = false;
    };
    std::array<Button, static_cast<std::size_t>(InputAction::Count)> buttons{};
    float Axis(InputAction negative, InputAction positive) const
    {
        return static_cast<float>(Held(positive)) - static_cast<float>(Held(negative));
    }
    bool cameraControl = false;
    bool enabled = true;
    bool interrupted = false;
    std::uint64_t interruptionCount = 0;
};
