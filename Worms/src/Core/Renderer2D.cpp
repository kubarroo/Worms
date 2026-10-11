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
    float width, height;
    SDL_CALL(SDL_GetTextureSize(texture, &width, &height));
    return {static_cast<int>(width), static_cast<int>(height)};
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
    SDL_FRect dest{float(destination.x), float(destination.y), float(destination.w), float(destination.h)};
    SDL_FRect src{};
    SDL_FPoint pivot{};
    if (source)
        src = {float(source->x), float(source->y), float(source->w), float(source->h)};
    if (center)
        pivot = {float(center->x), float(center->y)};
    SDL_CALL(SDL_RenderTextureRotated(renderer, texture, source ? &src : nullptr, &dest, angle,
                              center ? &pivot : nullptr, SDL_FLIP_NONE));
}
void Renderer2D::DrawLines(std::span<const RenderPoint> points, RenderColor color) const
{
    RequireRenderer();
    if (points.size() < 2)
        return;
    std::vector<SDL_FPoint> native;
    native.reserve(points.size());
    for (const auto point : points)
        native.push_back({float(point.x), float(point.y)});
    SDL_CALL(SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a));
    SDL_CALL(SDL_RenderLines(renderer, native.data(), static_cast<int>(native.size())));
}
void Renderer2D::DrawLine(RenderPoint first, RenderPoint second, RenderColor color) const
{
    RequireRenderer();
    SDL_CALL(SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a));
    SDL_CALL(SDL_RenderLine(renderer, float(first.x), float(first.y), float(second.x), float(second.y)));
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
    SDL_CALL(SDL_RenderPresent(renderer));
}
void Renderer2D::SetLogicalSize(int width, int height) const
{
    RequireRenderer();
    SDL_CALL(SDL_SetRenderLogicalPresentation(renderer, width, height,
        width == 0 && height == 0 ? SDL_LOGICAL_PRESENTATION_DISABLED
                                 : SDL_LOGICAL_PRESENTATION_LETTERBOX));
}
void Renderer2D::SetScale(float x, float y) const
{
    RequireRenderer();
    SDL_CALL(SDL_SetRenderScale(renderer, x, y));
}
