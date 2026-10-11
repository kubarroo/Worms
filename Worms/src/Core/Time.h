#pragma once
#include <SDL3/SDL.h>

namespace Time
{
extern double deltaTime;
void ResetFrameClock() noexcept;
void UpdateFrameTime() noexcept;

// Limit unpresented frames and the fallback loop without double-limiting VSync.
constexpr Uint64 FrameDelayNS(Uint64 elapsedNS, bool vsyncEnabled, bool presented) noexcept
{
    constexpr Uint64 frameNS = SDL_NS_PER_SECOND / 60;
    return (vsyncEnabled && presented) || elapsedNS >= frameNS ? 0 : frameNS - elapsedNS;
}

class Timer
{
public:
    Timer();
    double Reset();
    double Measure();

private:
    uint64_t lastTime = 0;
    double deltaTime = 0.0;
};
}; // namespace Time
