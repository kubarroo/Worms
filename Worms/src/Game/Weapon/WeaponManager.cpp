#include "WeaponManager.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <stdexcept>
#include <utility>

void WeaponManager::LoadTexture(const std::string& path)
{
    if (textures.contains(path))
        return;

    TexturePtr texture(IMG_LoadTexture(renderer, path.c_str()), &SDL_DestroyTexture);
    SDL_CHECK(texture.get());
    textures.emplace(path, std::move(texture));
}

void WeaponManager::LoadSound(const std::string& path)
{
    if (path.empty() || sounds.contains(path))
        return;

    sounds.emplace(path, std::make_unique<Sound>(path));
}

void WeaponManager::ApplyCurrentWeapon()
{
    const auto& params = *weapons.at(currentWeapon);
    weapon->SetParams(params);
    weapon->SetTexture(textures.at(params.weaponTexturePath).get());
    weapon->SetProjectileTexture(textures.at(params.projectileTexturePath).get());
    weapon->SetExplosionSound(params.explosionSound.empty()
                                 ? nullptr : sounds.at(params.explosionSound).get());
    weapon->SetShootingSound(params.shootingSound.empty()
                                ? nullptr : sounds.at(params.shootingSound).get());
    weapon->SetCollisionSound(params.collisionSound.empty()
                                 ? nullptr : sounds.at(params.collisionSound).get());
}

void WeaponManager::Initialise()
{
    if (initialized)
        throw std::logic_error("WeaponManager is already initialized");
    if (!renderer || !weapon || !weapon->HasEntity())
        throw std::logic_error("WeaponManager requires a renderer and an initialized weapon");

    if (weapons.empty())
    {
        std::vector<std::unique_ptr<WeaponImpl>> configs;
        configs.push_back(std::make_unique<Grenade>());
        configs.push_back(std::make_unique<Bazooka>());
        weapons = std::move(configs);
    }

    for (const auto& params : weapons)
    {
        LoadTexture(params->weaponTexturePath);
        LoadTexture(params->projectileTexturePath);
        LoadSound(params->explosionSound);
        LoadSound(params->collisionSound);
        LoadSound(params->shootingSound);
    }

    currentWeapon = 0;
    ApplyCurrentWeapon();
    initialized = true;
}

void WeaponManager::Update()
{
    if (!initialized)
        return;

    currentWeapon += Input::Get().ChangeWeapon();
    if (currentWeapon < 0)
        currentWeapon = static_cast<int>(weapons.size()) - 1;
    if (currentWeapon >= static_cast<int>(weapons.size()))
        currentWeapon = 0;

    ApplyCurrentWeapon();
}
