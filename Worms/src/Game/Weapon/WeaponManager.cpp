#include "WeaponManager.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <stdexcept>
#include <utility>

WeaponManager::WeaponManager(ResourceManager& resources, Weapon& weapon, Input& input)
    : resources(resources), input(input), weapon(&weapon)
{
}

void WeaponManager::ApplyCurrentWeapon()
{
    const auto& params = *weapons.at(currentWeapon);
    weapon->SetParams(params);
    weapon->SetTexture(resources.GetTexture(params.weaponTexturePath));
    weapon->SetProjectileTexture(resources.GetTexture(params.projectileTexturePath));
    weapon->SetExplosionSound(params.explosionSound.empty()
                                 ? nullptr : &resources.GetSound(params.explosionSound));
    weapon->SetShootingSound(params.shootingSound.empty()
                                ? nullptr : &resources.GetSound(params.shootingSound));
    weapon->SetCollisionSound(params.collisionSound.empty()
                                 ? nullptr : &resources.GetSound(params.collisionSound));
}

void WeaponManager::Initialise()
{
    if (initialized)
        throw std::logic_error("WeaponManager is already initialized");
    if (!resources.Renderer() || !weapon || !weapon->HasEntity())
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
        resources.GetTexture(params->weaponTexturePath);
        resources.GetTexture(params->projectileTexturePath);
        for (const auto* path : {&params->explosionSound, &params->collisionSound,
                                &params->shootingSound})
            if (!path->empty()) resources.GetSound(*path);
    }

    currentWeapon = 0;
    ApplyCurrentWeapon();
    initialized = true;
}

void WeaponManager::Update()
{
    if (!initialized)
        return;

    currentWeapon += input.ChangeWeapon();
    if (currentWeapon < 0)
        currentWeapon = static_cast<int>(weapons.size()) - 1;
    if (currentWeapon >= static_cast<int>(weapons.size()))
        currentWeapon = 0;

    ApplyCurrentWeapon();
}
