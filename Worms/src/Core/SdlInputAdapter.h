#pragma once
#include "Input.h"
union SDL_Event;
class SdlInputAdapter
{
public:
    explicit SdlInputAdapter(Input& input) : input(input) {}
    void BeginFrame(bool keyboardCaptured) noexcept;
    void ProcessEvent(const SDL_Event& event, bool keyboardCaptured) noexcept;
    void Reset() noexcept;

private:
    Input& input;
    bool focused = true;
};
