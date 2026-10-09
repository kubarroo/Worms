#include "Game/Player/HealthBar.h"
#include "ECS/World.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <SDL_image.h>

HealthBar::HealthBar(SDL_Renderer* renderer, World* world, EntityId newParentId,
                     const Camera& camera, int health, SDL_Texture* texture)
    : camera(camera)
{
    try
    {
        SDL_CHECK(texture);
        Initialise(renderer, world);

        world->AddComponent<Position>(objectId, {0, 0});
        world->AddComponent<Health>(objectId, {100, 100});
        world->AddComponent<Follow>(objectId, {newParentId, 0.0, 0.3});
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
    SDL_Point size;
    SDL_QueryTexture(healthBar, NULL, NULL, &size.x, &size.y);

    double hpPrc = static_cast<float>(hp.current) / static_cast<float>(hp.max);
    SDL_Rect slice(0, 0, static_cast<int>(size.x) * hpPrc, size.y);

    SDL_Rect renderQuad(400 + static_cast<int>((pos.x - camera.X()) * 100.0) - size.x / 2,
                        300 - static_cast<int>((pos.y - camera.Y()) * 100.0) - size.y / 2, slice.w,
                        slice.h);

    SDL_RenderCopy(renderer, healthBar, &slice, &renderQuad);
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
