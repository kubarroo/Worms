#pragma once
#include "Game/Player/WormTeam.h"
#include "Game/Weapon/Weapon.h"
#include <box2d/b2_world.h>


class WormManager
{
public:
    WormManager(SDL_Renderer* renderer, World* world, b2World* physicsWorld, Camera& camera,
                Weapon& weapon);
    WormManager(const WormManager&) = delete;
    WormManager(WormManager&&) = delete;

    void CreateTeam(int size);
    void DeleteTeam(WormTeam* team);
    EntityId GetActiveWormId()
    {
        return teams[activeTeam]->GetActiveWorm();
    }
    void RenderHealthBars();
    void Update();

    ~WormManager();

private:
    void ActiveWormCheck();
    void ChangeTeam();
    void ChangeActiveWorm();

    std::vector<WormTeam*> teams;
    int activeTeam = 0;

    SDL_Renderer* renderer;
    World* world;
    b2World* physicsWorld;
    Camera& camera;
    Weapon& weapon;
};