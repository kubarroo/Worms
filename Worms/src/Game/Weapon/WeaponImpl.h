#pragma once
#include <string>

struct WeaponImpl
{
	virtual ~WeaponImpl() = default;
	std::string weaponTexturePath = "placeHolderWeapon.png";
	std::string projectileTexturePath = "placeHolderBullet.png";;
	std::string explosionSound;
	std::string collisionSound;
	std::string shootingSound;

	float explosionOffset = 0;
	float explosionRadius = 1.f;
	float baseDamage = 40.f;
	float gravityScale = 1.f;
	float maxSpeed = 2.5f;
	float bounciness = 0.f;
};
