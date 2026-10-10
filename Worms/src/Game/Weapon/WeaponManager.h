#pragma once
#include <map>
#include <memory>
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
    WeaponManager(SDL_Renderer* renderer, Camera& camera, GameScene& scene);

	void Initialise();
	void Update();
	Weapon* GetWeapon() const { return weapon; }
	~WeaponManager() = default;
private:
	using TexturePtr = Sdl::TexturePtr;
	void LoadTexture(const std::string& path);
	void LoadSound(const std::string& path);
	void ApplyCurrentWeapon();
	SDL_Renderer* renderer;
	Weapon* weapon = nullptr;
	int currentWeapon = 0;
	bool initialized = false;
	std::vector<std::unique_ptr<WeaponImpl>> weapons;
	std::map<std::string, std::unique_ptr<Sound>> sounds;
	std::map<std::string, TexturePtr> textures;

};

