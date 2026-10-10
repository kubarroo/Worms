#pragma once
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Game/Components.h"

class HealthBar : public GameObject
{
public:
    HealthBar(EntityId parentId, const Camera& camera, int health, SDL_Texture* texture);
    void Initialise(const SceneContext& context) override;
    void Render() override;
    void CleanUp() override;
    void TakeDamage(int amount);
    int getCurrentHp()
    {
        if (!HasEntity()) throw std::logic_error("HealthBar has no entity");
        return world->GetComponent<Health>(objectId).current;
    }

private:
    EntityId parentId;
    int initialHealth;
    // Both pointers borrow the team texture; WormTeam outlives its health bars.
    SDL_Texture* texture;
    SDL_Texture* healthBar = nullptr;
    const Camera& camera;
};
