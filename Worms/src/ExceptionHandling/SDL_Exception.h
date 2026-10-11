#pragma once
#include "AppException.h"
#include "SDL3/SDL.h"

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

#define SDL_CALL(x) do { if (!(x)) throw SDL_Exception(__LINE__, __FILE__); } while (false)
#define SDL_CHECK(x) do { if (!(x)) throw SDL_Exception(__LINE__, __FILE__); } while (false)
