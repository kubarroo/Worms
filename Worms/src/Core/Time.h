#pragma once
#include <SDL2/SDL.h>

namespace Time
{
extern double deltaTime;
void ResetFrameClock() noexcept;
void UpdateFrameTime() noexcept;

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
