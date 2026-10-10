#pragma once
#include "AppException.h"
#include "SDL2/SDL.h"

class SDL_Exception : public AppException
{
public:
    SDL_Exception(int line, const char* file) noexcept : AppException(line, file, SDL_GetError()) {}
    SDL_Exception(int line, const char* file, const char* message) noexcept
        : AppException(line, file, message) {}

    virtual inline const char* GetType() const noexcept
    {
        return "SDL Exception";
    }
};

#define SDL_CALL(x)                                                                                \
    SDL_ClearError();                                                                              \
    if (x != 0)                                                                                    \
    throw SDL_Exception(__LINE__, __FILE__)
#define SDL_CHECK(x)                                                                               \
    if (x == NULL)                                                                                 \
    throw SDL_Exception(__LINE__, __FILE__)
