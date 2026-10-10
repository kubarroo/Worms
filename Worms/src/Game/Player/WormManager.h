#pragma once
#include "Game/Player/WormTeam.h"
#include "Game/Weapon/Weapon.h"
#include <box2d/b2_world.h>
#include <memory>

class WormManager
{
public:
    WormManager(SDL_Renderer* renderer, World* world, b2World* physicsWorld, Camera& camera,
                Weapon& weapon, GameScene* scene = nullptr);
    WormManager(const WormManager&) = delete;
    WormManager(WormManager&&) = delete;

    void CreateTeam(int size);
    void DeleteTeam(WormTeam* team);
    EntityId GetActiveWormId()
    {
        if (teams.empty())
        {
            throw std::logic_error("No active team");
        }

        ActiveTeamCheck();
        return teams[activeTeam]->GetActiveWorm();
    }
    void RenderHealthBars();
    void Update();
    void CleanUp();

    ~WormManager() = default;

private:
    void ActiveTeamCheck();
    void ChangeTeam();
    void ChangeActiveWorm();

    std::vector<std::unique_ptr<WormTeam>> teams;
    int activeTeam = 0;
    bool nextTeamAlreadySelected = false;

    SDL_Renderer* renderer;
    World* world;
    b2World* physicsWorld;
    Camera& camera;
    Weapon& weapon;
    GameScene* scene;
};
