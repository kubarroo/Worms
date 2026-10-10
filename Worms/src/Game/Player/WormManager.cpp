#include "Game/Player/WormManager.h"
#include "Core/Input.h"
#include "ECS/ECS_Types.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Player/WormTeam.h"
#include <SDL_stdinc.h>
#include <memory>
#include <stdexcept>
#include <utility>

SDL_Texture* createTexture(int team, SDL_Renderer* renderer)
{
    int width = 40;
    int height = 10;
    Sdl::SurfacePtr surface(
        SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000));
    if (!surface)
    {
        return nullptr;
    }

    Uint8 colors[4][3] = {
        {0, 255, 0},   // team 0: green
        {255, 0, 0},   // team 1: red
        {255, 255, 0}, // team 2: yellow
        {0, 255, 255}  // team 3: cyan
    };

    Uint8 r = 255, g = 255, b = 255; // default: white
    if (team < 4)
    {
        r = colors[team][0];
        g = colors[team][1];
        b = colors[team][2];
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const float prc = 0.4f + (static_cast<float>(x) / width) * 0.5f;
            Uint32 color = SDL_MapRGB(surface->format, static_cast<Uint8>(r * prc),
                                      static_cast<Uint8>(g * prc), static_cast<Uint8>(b * prc));
            Uint32* pixels = (Uint32*)surface->pixels;
            pixels[(y * surface->w) + x] = color;
        }
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface.get());
    return texture;
};

WormManager::WormManager(const SceneContext& context, Camera& camera, Weapon& weapon)
    : context(context), camera(camera), weapon(weapon)
{
}

void WormManager::Initialise()
{
    if (initialized)
        throw std::logic_error("WormManager is already initialized");
    initialized = true;
}

void WormManager::OnCameraTargetLost()
{
    if (!initialized)
        return;

    if (teams.empty())
    {
        return;
    }

    if (!nextTeamAlreadySelected)
    {
        ChangeTeam();
    }

    nextTeamAlreadySelected = false;

    const EntityId wormId = GetActiveWormId();
    weapon.SetParent(wormId);
    camera.ChangeTarget(wormId);
    weapon.Activate();
}

void WormManager::CreateTeam(int size)
{
    if (!initialized)
        throw std::logic_error("WormManager is not initialized");
    if (size <= 0)
    {
        throw std::invalid_argument("Team must contain at least one worm");
    }

    WormTeam::TexturePtr texture(createTexture(static_cast<int>(teams.size()), context.renderer));

    SDL_CHECK(texture.get());

    auto newTeam = std::make_unique<WormTeam>(std::move(texture));

    try
    {
        newTeam->Initialise(context.resources);
        for (int i = 0; i < size; ++i)
        {
            const Position spawnPosition{
                -1.f + static_cast<float>(spawnedWorms) + static_cast<float>(i), 2.f};
            auto worm =
                std::make_unique<Worm>(camera, newTeam->GetHealthBarTexture(), spawnPosition);
            worm->Initialise(context);
            newTeam->AddWorm(std::move(worm));
        }

        teams.push_back(std::move(newTeam));
        spawnedWorms += static_cast<std::size_t>(size);
    }
    catch (...)
    {
        if (newTeam)
        {
            newTeam->CleanUp();
        }
        throw;
    }
}

void WormManager::DeleteTeam(WormTeam* team)
{
    auto it = std::find_if(teams.begin(), teams.end(),
                           [team](const auto& item) { return item.get() == team; });
    if (it == teams.end())
    {
        return;
    }

    const int removedIndex = static_cast<int>(it - teams.begin());
    const bool removedActiveTeam = removedIndex == activeTeam;

    (*it)->CleanUp();
    teams.erase(it);

    if (removedIndex < activeTeam)
    {
        --activeTeam;
    }

    ActiveTeamCheck();

    if (teams.empty())
    {
        nextTeamAlreadySelected = false;
        weapon.ClearParent();
        camera.ClearTarget();
    }
    else if (removedActiveTeam)
    {
        nextTeamAlreadySelected = true;
        weapon.Deactivate();
        weapon.SetParent(GetActiveWormId());
    }
}

void WormManager::ChangeTeam()
{
    if (teams.empty())
    {
        weapon.ClearParent();
        camera.ClearTarget();
        return;
    }

    activeTeam = (activeTeam + 1) % static_cast<int>(teams.size());
    ChangeActiveWorm();

    const EntityId activeWorm = GetActiveWormId();
    weapon.SetParent(activeWorm);
    camera.ChangeTarget(activeWorm);
}

void WormManager::ChangeActiveWorm()
{
    if (teams.empty())
    {
        return;
    }

    ActiveTeamCheck();
    teams[activeTeam]->ChangeActiveWorm();

    const EntityId activeWorm = GetActiveWormId();
    weapon.SetParent(activeWorm);
    camera.ChangeTarget(activeWorm);
}

void WormManager::Update()
{
    if (!initialized)
        return;
    if (!nextTeamAlreadySelected)
    {
        if (Input::Get().ChangeWorm())
        {
            ChangeActiveWorm();
        }
        if (Input::Get().ChangeTeam())
        {
            ChangeTeam();
        }
    }
    if (teams.empty())
    {
        weapon.ClearParent();
        camera.ClearTarget();
        return;
    }

    teams[activeTeam]->Update();

    if (teams[activeTeam]->Size() == 0)
    {
        DeleteTeam(teams[activeTeam].get());
    }
    ActiveTeamCheck();
    if (!teams.empty())
    {
        weapon.SetParent(GetActiveWormId());
    }
    else
    {
        weapon.ClearParent();
        camera.ClearTarget();
    }
}

void WormManager::RenderHealthBars()
{
    for (const auto& team : teams)
    {
        team->RenderHealthBars();
    }
}

void WormManager::CleanUp()
{
    if (!initialized)
        return;
    nextTeamAlreadySelected = false;
    camera.ClearTarget();
    weapon.ClearParent();

    for (auto& team : teams)
    {
        team->CleanUp();
    }

    teams.clear();
    activeTeam = 0;
    spawnedWorms = 0;
    initialized = false;
}

void WormManager::ActiveTeamCheck()
{
    if (activeTeam >= teams.size())
    {
        activeTeam = 0;
    }
}
