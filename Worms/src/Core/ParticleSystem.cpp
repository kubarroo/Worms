#include "ParticleSystem.h"

ParticleSystem::ParticleSystem(std::string particleImg, float startScale, float startPosX,
                               float startPosY, int amountOfParticles)
    : particleImg(particleImg), startScale(startScale), startPosX(startPosX), startPosY(startPosY),
      amountOfParticles(amountOfParticles)
{
    srand(time(NULL));
}

void ParticleSystem::CleanUp()
{
    for (const auto particle : particles)
        world->DestroyEntity(particle);
}

void ParticleSystem::Update()
{
    timer += Time::deltaTime;
    progress = timer / length;
    if (timer < length)
        return;

    SDL_DestroyTexture(world->GetComponent<Sprite>(particles.front()).texture);
    GameObject::objsToDelete.push_back(this);
}

void ParticleSystem::Initialise(SDL_Renderer* newRenderer, World* newWorld)
{
    GameObject::Initialise(newRenderer, newWorld);
    SDL_Texture* texture = IMG_LoadTexture(renderer, particleImg.c_str());
    SDL_CHECK(texture);

    for (int i = 0; i < amountOfParticles; i++)
    {
        EntityId particleId = world->CreateEntity();
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
        particles.emplace_back(particleId);
    }
}

float ParticleSystem::easeOutCubic(float x)
{
    return 1 - pow(1 - x, 3);
}
