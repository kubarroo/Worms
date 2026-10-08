#pragma once
#include <map>
#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <vector>
#include "Core/Audio/Sound.h"
#include "Core/Input.h"
#include "Weapon.h"
#include "WeaponTypes.h"

class WeaponManager
{
public:
	WeaponManager( SDL_Renderer* renderer, Camera& camera ) : renderer( renderer )
	{
		auto weapon = std::make_unique<Weapon>( camera );
		this->weapon = weapon.get();
		GameObject::activeObjs.emplace_back( std::move( weapon ) );
	}

	void Initialise();
	void Update();
	Weapon* GetWeapon() const { return weapon; }
	~WeaponManager();
private:
	SDL_Renderer* renderer;
	Weapon* weapon;
	int currentWeapon = 0;
	std::vector<WeaponImpl*> weapons;
	std::map<std::string, Sound*> sounds;
	std::map<std::string, SDL_Texture*> textures;

};

