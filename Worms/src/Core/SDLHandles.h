#pragma once
#include <SDL2/SDL.h>
#include "ExceptionHandling/SDL_Exception.h"
#include <memory>

namespace Sdl
{
struct WindowDeleter
{
    void operator()(SDL_Window* window) const noexcept { SDL_DestroyWindow(window); }
};
struct RendererDeleter
{
    void operator()(SDL_Renderer* renderer) const noexcept { SDL_DestroyRenderer(renderer); }
};
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
using WindowPtr = std::unique_ptr<SDL_Window, WindowDeleter>;
using RendererPtr = std::unique_ptr<SDL_Renderer, RendererDeleter>;

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
