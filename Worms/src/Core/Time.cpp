#include "Core/Time.h"

namespace Time
{
double deltaTime = 0.0;

namespace
{
uint64_t lastFrameTime = 0;
bool frameClockStarted = false;
}

void ResetFrameClock() noexcept
{
    deltaTime = 0.0;
    lastFrameTime = 0;
    frameClockStarted = false;
}

void UpdateFrameTime() noexcept
{
    const auto now = SDL_GetTicks64();
    deltaTime = frameClockStarted ? (now - lastFrameTime) / 1000.0 : 0.0;
    lastFrameTime = now;
    frameClockStarted = true;
}

Timer::Timer()
{
    lastTime = SDL_GetTicks64();
}

double Timer::Reset()
{
    uint64_t newTime = SDL_GetTicks64();
    deltaTime = (newTime - lastTime) / 1000.0;
    lastTime = newTime;
    return deltaTime;
}

double Timer::Measure()
{
    uint64_t newTime = SDL_GetTicks64();
    deltaTime = (newTime - lastTime) / 1000.0;
    return deltaTime;
}
}; // namespace Time
