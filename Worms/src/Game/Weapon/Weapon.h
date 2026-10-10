#pragma once
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/SDLHandles.h"
#include "Game/Components.h"
#include "Game/Weapon/Projectile.h"
#include "WeaponImpl.h"
#include <optional>
#include <memory>
#include <vector>

class Weapon : public GameObject
{
public:
    using TexturePtr = Sdl::TexturePtr;
    Weapon(Camera& camera);

    void Initialise(const SceneContext& context) override;
    void Update() override;
    void Render() override;
    void CleanUp() override;
    void Activate()
    {
        canShoot = true;
    }
    void SetParent(EntityId newParent)
    {
        const auto newHandle = HasEntity() ? world->GetHandle(newParent) : std::nullopt;
        if (parentId == newHandle) return;
        ClearParent();
        parentId = newHandle;
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
    friend struct WeaponTestAccess;
    std::optional<Position> GetParentPosition();
    std::optional<EntityHandle> parentId;
    float force = 0;
    bool canShoot = true;
    TexturePtr powerBar;
    SDL_Texture* projTexture = nullptr;
    Sound* explosionSound = nullptr;
    Sound* collisionSound = nullptr;
    Sound* shootingSound = nullptr;

    Camera& camera;
    WeaponImpl weaponParams;
};
