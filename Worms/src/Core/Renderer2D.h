#pragma once
#include <cstdint>
#include <optional>
#include <span>

struct SDL_Renderer;
struct SDL_Texture;
using TextureHandle = SDL_Texture*;
struct RenderPoint
{
    int x = 0, y = 0;
};
struct RenderRect
{
    int x = 0, y = 0, w = 0, h = 0;
};
struct RenderColor
{
    std::uint8_t r = 0, g = 0, b = 0, a = 255;
};

// Borrows the platform renderer and never changes shared texture state.
class Renderer2D
{
public:
    explicit Renderer2D(SDL_Renderer* renderer) : renderer(renderer) {}
    SDL_Renderer* Native() const noexcept
    {
        return renderer;
    }
    RenderPoint TextureSize(TextureHandle texture) const;
    void DrawSprite(TextureHandle texture, RenderRect destination,
                    std::optional<RenderRect> source = {}, double angle = 0,
                    std::optional<RenderPoint> center = {}) const;
    void DrawLines(std::span<const RenderPoint> points, RenderColor color) const;
    void DrawLine(RenderPoint first, RenderPoint second, RenderColor color) const;
    void Clear(RenderColor color) const;
    void Present() const;
    void SetLogicalSize(int width, int height) const;
    void SetScale(float x, float y) const;

private:
    void RequireRenderer() const;
    SDL_Renderer* renderer;
};
