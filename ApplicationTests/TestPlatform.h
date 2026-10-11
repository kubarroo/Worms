#pragma once
#include <SDL3/SDL.h>
#include <initializer_list>
#include <utility>
#include "ExceptionHandling/SDL_Exception.h"

inline void ConfigureTestDrivers()
{
    // SDL_Quit clears hints. Restore defaults on every test platform restart,
    // preserving explicit overrides used to exercise startup failures.
    for (const auto& setting : {std::pair{SDL_HINT_VIDEO_DRIVER, "dummy"},
                                std::pair{SDL_HINT_AUDIO_DRIVER, "dummy"},
                                std::pair{SDL_HINT_RENDER_DRIVER, "software"}})
        if (!SDL_GetHint(setting.first))
            SDL_CALL(SDL_SetHintWithPriority(setting.first, setting.second, SDL_HINT_OVERRIDE));
}
