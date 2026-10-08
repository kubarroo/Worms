#pragma once
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Game/Components.h"

class HealthBar : public GameObject
{
public:
    HealthBar(SDL_Renderer* renderer, World* world, EntityId newParentId, const Camera& camera,
              int health, SDL_Texture* texture);
    void Render() override;
    void TakeDamage(int amount);
    int getCurrentHp()
    {
        return world->GetComponent<Health>(objectId).current;
    }

private:
    SDL_Texture* healthBar;
    const Camera& camera;
};