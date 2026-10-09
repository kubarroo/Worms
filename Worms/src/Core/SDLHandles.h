#pragma once
#include <SDL2/SDL.h>
#include "ExceptionHandling/SDL_Exception.h"
#include <memory>

namespace Sdl
{
struct TextureDeleter
{
    void operator()(SDL_Texture* texture) const noexcept { SDL_DestroyTexture(texture); }
};
struct SurfaceDeleter
{
    void operator()(SDL_Surface* surface) const noexcept { SDL_FreeSurface(surface); }
};
using TexturePtr = std::unique_ptr<SDL_Texture, TextureDeleter>;
using SurfacePtr = std::unique_ptr<SDL_Surface, SurfaceDeleter>;

class SurfaceLock
{
public:
    explicit SurfaceLock(SDL_Surface* surface) : surface(surface)
    {
        SDL_CALL(SDL_LockSurface(surface));
    }
    ~SurfaceLock() { SDL_UnlockSurface(surface); }
    SurfaceLock(const SurfaceLock&) = delete;
    SurfaceLock& operator=(const SurfaceLock&) = delete;
private:
    SDL_Surface* surface;
};
}
