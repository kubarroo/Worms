#include "Game/Player/WormManager.h"
#include "Core/Input.h"


SDL_Texture* createTexture(int team, SDL_Renderer* renderer)
{
    float width = 40.;
    float height = 10.;
    SDL_Surface* surface =
        SDL_CreateRGBSurface(0, width, height, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
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
            float prc = 0.4 + ((x / width) * 0.5);
            Uint32 color = SDL_MapRGB(surface->format, r * prc, g * prc, b * prc);
            Uint32* pixels = (Uint32*)surface->pixels;
            pixels[(y * surface->w) + x] = color;
        }
    }
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    return texture;
};

WormManager::WormManager(SDL_Renderer* renderer, World* world, b2World* physicsWorld,
                         Camera& camera, Weapon& weapon)
    : renderer(renderer), world(world), teams(), physicsWorld(physicsWorld), camera(camera),
      weapon(weapon)
{
    camera.noTargetEvent = [&]()
    {
        ChangeTeam();
        if (teams.size() != 0)
        {
            camera.ChangeTarget(teams[activeTeam]->GetActiveWorm());
            weapon.Activate();
        }
    };
}

void WormManager::CreateTeam(int size)
{
    WormTeam* newTeam = new WormTeam;

    SDL_Texture* texture = createTexture(teams.size(), renderer);

    for (int i = 0; i < size; i++)
    {
        newTeam->AddWorm(new Worm(renderer, world, physicsWorld, camera, texture));
    }
    teams.push_back(newTeam);
}

void WormManager::DeleteTeam(WormTeam* team)
{
    teams.erase(std::remove_if(teams.begin(), teams.end(),
                               [team](WormTeam* current) { return team == current; }));
}

void WormManager::ChangeTeam()
{
    if (teams.empty())
        return;

    activeTeam = (activeTeam + 1) % teams.size();
    ChangeActiveWorm();
    camera.ChangeTarget(teams[activeTeam]->GetActiveWorm());
}

void WormManager::ChangeActiveWorm()
{
    if (!teams.empty())
        teams[activeTeam]->ChangeActiveWorm();
}

void WormManager::Update()
{
    if (Input::Get().ChangeWorm())
        ChangeActiveWorm();
    if (Input::Get().ChangeTeam())
        ChangeTeam();
    if (teams.empty())
        return;

    teams[activeTeam]->Update();

    if (teams[activeTeam]->Size() == 0)
    {
        DeleteTeam(teams[activeTeam]);
    }
    ActiveWormCheck();
    if (!teams.empty())
        weapon.SetParent(GetActiveWormId());
}

void WormManager::RenderHealthBars()
{
    for (auto team : teams)
    {
        team->RenderHealthBars();
    }
}

WormManager::~WormManager()
{
    if (!teams.empty())
    {
        for (auto& team : teams)
        {
            delete team;
            team = nullptr;
        }
        teams.clear();
    }
}

void WormManager::ActiveWormCheck()
{
    if (activeTeam >= teams.size())
    {
        activeTeam = 0;
    }
}