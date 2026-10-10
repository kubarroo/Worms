#pragma once
#include <memory>
#include <SDL2/SDL.h>
#include "Core/ResourceManager.h"
#include <vector>
#include "Core/Audio/Sound.h"
#include "Core/Input.h"
#include "Weapon.h"
#include "WeaponTypes.h"

class WeaponManager
{
public:
    WeaponManager(ResourceManager& resources, Weapon& weapon, Input& input);

    void Initialise();
	void Update();
	Weapon* GetWeapon() const { return weapon; }
	~WeaponManager() = default;
private:
	void ApplyCurrentWeapon();
    ResourceManager& resources;
    Input& input;
	Weapon* weapon = nullptr;
	int currentWeapon = 0;
	bool initialized = false;
	std::vector<std::unique_ptr<WeaponImpl>> weapons;

};

