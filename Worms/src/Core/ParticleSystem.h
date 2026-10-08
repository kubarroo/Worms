#pragma once
#include <random>
#include "Core/GameObject.h"
#include "Core/Time.h"
#include "ECS/System.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Components.h"
#include "SDL_image.h"

class ParticleSystem : public GameObject
{
public:
	ParticleSystem( std::string particleImg, float startScale, float startPosX, float startPosY, int amountOfParticles );

	void CleanUp() final;
	void Update() final;
	void Initialise( SDL_Renderer* newRenderer, World* newWorld ) final;

private:
	float easeOutCubic( float x );
	float startPosX, startPosY;
	std::string particleImg;
	float startScale;
	int amountOfParticles;
	float timer = 0.f;
	float length = 5.f;
	float progress;
	std::vector<EntityId> particles;
};
