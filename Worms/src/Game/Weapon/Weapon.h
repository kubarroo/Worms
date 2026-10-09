#pragma once
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Game/Components.h"
#include "Game/Weapon/Projectile.h"
#include "WeaponImpl.h"
#include <optional>
#include <memory>
#include <vector>

class Weapon : public GameObject
{
public:
    using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    Weapon(Camera& camera);

    void Initialise(SDL_Renderer* newRenderer, World* newWorld) override;
    void Update() override;
    void Render() override;
    void Activate()
    {
        canShoot = true;
    }
    void SetParent(EntityId newParent)
    {
        parentId = newParent;
    }
    void ClearParent()
    {
        parentId.reset();
        force = 0;
    }
    void SetParams(WeaponImpl params)
    {
        weaponParams = params;
    }
    void SetTexture(SDL_Texture* texture)
    {
        world->GetComponent<Sprite>(objectId).texture = texture;
    }
    void SetExplosionSound(Sound* sound)
    {
        explosionSound = sound;
    }
    void SetCollisionSound(Sound* sound)
    {
        collisionSound = sound;
    }
    void SetShootingSound(Sound* sound)
    {
        shootingSound = sound;
    }
    void SetProjectileTexture(SDL_Texture* texture)
    {
        projTexture = texture;
    }
    void Deactivate()
    {
        canShoot = false;
        force = 0.f;
    }

private:
    std::optional<EntityId> parentId;
    float force = 0;
    bool canShoot = true;
    TexturePtr powerBar{nullptr, &SDL_DestroyTexture};
    SDL_Texture* projTexture = nullptr;
    Sound* explosionSound = nullptr;
    Sound* collisionSound = nullptr;
    Sound* shootingSound = nullptr;

    Camera& camera;
    WeaponImpl weaponParams;
};
