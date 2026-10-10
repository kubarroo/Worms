#include "Renderer2D.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <stdexcept>
#include <vector>

void Renderer2D::RequireRenderer() const
{
    if (!renderer)
        throw std::logic_error("Drawing requires a live renderer");
}
RenderPoint Renderer2D::TextureSize(TextureHandle texture) const
{
    if (!texture)
        throw std::invalid_argument("Texture must not be null");
    RenderPoint size;
    SDL_CALL(SDL_QueryTexture(texture, nullptr, nullptr, &size.x, &size.y));
    return size;
}
void Renderer2D::DrawSprite(TextureHandle texture, RenderRect destination,
                            std::optional<RenderRect> source, double angle,
                            std::optional<RenderPoint> center) const
{
    RequireRenderer();
    if (!texture)
        throw std::invalid_argument("Texture must not be null");
    if (destination.w <= 0 || destination.h <= 0 || (source && (source->w <= 0 || source->h <= 0)))
        return;
    SDL_Rect dest{destination.x, destination.y, destination.w, destination.h};
    SDL_Rect src{};
    SDL_Point pivot{};
    if (source)
        src = {source->x, source->y, source->w, source->h};
    if (center)
        pivot = {center->x, center->y};
    SDL_CALL(SDL_RenderCopyEx(renderer, texture, source ? &src : nullptr, &dest, angle,
                              center ? &pivot : nullptr, SDL_FLIP_NONE));
}
void Renderer2D::DrawLines(std::span<const RenderPoint> points, RenderColor color) const
{
    RequireRenderer();
    if (points.size() < 2)
        return;
    std::vector<SDL_Point> native;
    native.reserve(points.size());
    for (const auto point : points)
        native.push_back({point.x, point.y});
    SDL_CALL(SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a));
    SDL_CALL(SDL_RenderDrawLines(renderer, native.data(), static_cast<int>(native.size())));
}
void Renderer2D::DrawLine(RenderPoint first, RenderPoint second, RenderColor color) const
{
    RequireRenderer();
    SDL_CALL(SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a));
    SDL_CALL(SDL_RenderDrawLine(renderer, first.x, first.y, second.x, second.y));
}
void Renderer2D::Clear(RenderColor color) const
{
    RequireRenderer();
    SDL_CALL(SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a));
    SDL_CALL(SDL_RenderClear(renderer));
}
void Renderer2D::Present() const
{
    RequireRenderer();
    SDL_RenderPresent(renderer);
}
void Renderer2D::SetLogicalSize(int width, int height) const
{
    RequireRenderer();
    SDL_CALL(SDL_RenderSetLogicalSize(renderer, width, height));
}
void Renderer2D::SetScale(float x, float y) const
{
    RequireRenderer();
    SDL_CALL(SDL_RenderSetScale(renderer, x, y));
}
