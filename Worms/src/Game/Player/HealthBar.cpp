#include "Game/Player/HealthBar.h"
#include "ECS/World.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Core/Renderer2D.h"
#include <algorithm>

HealthBar::HealthBar(EntityHandle parent, const Camera& camera, int health, SDL_Texture* texture)
    : parent(parent), initialHealth(health), texture(texture), camera(camera)
{
}

void HealthBar::Initialise(const SceneContext& context)
{
    if (!context.world.IsAlive(parent))
        throw std::invalid_argument("Health bar requires a live parent in its registry");
    GameObject::Initialise(context);
    try
    {
        SDL_CHECK(texture);

        world->AddComponent<Position>(objectId, {0, 0});
        world->AddComponent<Health>(objectId, {initialHealth, initialHealth});
        world->AddComponent<Follow>(objectId, {parent, 0.0, 0.3});
        healthBar = texture;
        SDL_CHECK(healthBar);
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}

void HealthBar::Render()
{
    if (!HasEntity() || !healthBar)
    {
        return;
    }
    auto& pos = world->GetComponent<Position>(objectId);
    auto& hp = world->GetComponent<Health>(objectId);
    auto size = Context().rendering.TextureSize(healthBar);

    double hpPrc = hp.max > 0 ? std::clamp(static_cast<double>(hp.current) / hp.max, 0.0, 1.0) : 0.0;
    RenderRect slice(0, 0, static_cast<int>(size.x * hpPrc), size.y);

    RenderRect renderQuad(400 + static_cast<int>((pos.x - camera.X()) * 100.0) - size.x / 2,
                        300 - static_cast<int>((pos.y - camera.Y()) * 100.0) - size.y / 2, slice.w,
                        slice.h);

    Context().rendering.DrawSprite(healthBar, renderQuad, slice);
}

void HealthBar::CleanUp()
{
    GameObject::CleanUp();
    healthBar = nullptr;
}

void HealthBar::TakeDamage(int amount)
{
    if (!HasEntity())
    {
        return;
    }
    world->GetComponent<Health>(objectId).current -= amount;
}
