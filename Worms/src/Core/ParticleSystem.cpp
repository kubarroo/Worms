#include "ParticleSystem.h"
#include <stdexcept>

ParticleSystem::ParticleSystem(std::string particleImg, float startScale, float startPosX,
                               float startPosY, int amountOfParticles)
    : particleImg(particleImg), startScale(startScale), startPosX(startPosX), startPosY(startPosY),
      amountOfParticles(amountOfParticles)
{
    if (amountOfParticles < 0)
        throw std::invalid_argument("Particle count cannot be negative");
    srand(time(NULL));
}

void ParticleSystem::CleanUp()
{
    if (!HasEntity())
    {
        return;
    }
    for (const auto particle : particles)
    {
        world->DestroyEntity(particle);
    }
    particles.clear();
    SDL_DestroyTexture(texture);
    texture = nullptr;
    GameObject::CleanUp();
}

void ParticleSystem::Update()
{
    if (!HasEntity())
    {
        return;
    }
    timer += Time::deltaTime;
    progress = timer / length;
    if (timer < length)
    {
        return;
    }

    GameObject::objsToDelete.push_back(this);
}

void ParticleSystem::Initialise(SDL_Renderer* newRenderer, World* newWorld)
{
    GameObject::Initialise(newRenderer, newWorld);
    texture = IMG_LoadTexture(renderer, particleImg.c_str());
    SDL_CHECK(texture);

    particles.reserve(static_cast<std::size_t>(amountOfParticles));

    for (int i = 0; i < amountOfParticles; i++)
    {
        EntityId particleId = world->CreateEntity();
        particles.emplace_back(particleId);
        b2Vec2 vel = {rand() * 2.f / RAND_MAX - 1.f, rand() * 2.f / RAND_MAX - 1.f};
        vel *= 1.f / sqrt(vel.x * vel.x + vel.y * vel.y);
        vel *= 0.012f * rand() / RAND_MAX;
        world->AddComponent<Particle>(particleId,
                                      {&progress, nullptr, [](double x, double y)
                                       { return std::pair<float, float>{x * 0.999, y * 0.999}; },
                                       [](double x) { return x * 0.98; }});
        world->AddComponent<Position>(particleId, {startPosX, startPosY});
        world->AddComponent<Motion>(particleId, {vel.x, vel.y});
        world->AddComponent<Sprite>(particleId, {texture});
        world->AddComponent<Scale>(particleId, {startScale * rand() / RAND_MAX});
    }
}

float ParticleSystem::easeOutCubic(float x)
{
    return 1 - pow(1 - x, 3);
}
