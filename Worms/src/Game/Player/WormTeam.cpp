#include "Game/Player/WormTeam.h"
#include "Core/ResourceManager.h"
#include <algorithm>
#include <stdexcept>

void WormTeam::Initialise(ResourceManager& resources)
{
    if (initialized)
        throw std::logic_error("WormTeam is already initialized");
    dieSound = &resources.GetSound("scream.wav");
    initialized = true;
}

void WormTeam::AddWorm(std::unique_ptr<Worm> worm)
{
    try
    {
        if (!initialized)
            throw std::logic_error("WormTeam is not initialized");
        if (!worm || !worm->HasEntity())
            throw std::invalid_argument("Team requires an initialized worm");
        worms.push_back(std::move(worm));
    }
    catch (...)
    {
        if (worm)
        {
            worm->CleanUp();
        }
        throw;
    }
    if (worms.size() == 1)
    {
        worms[0]->Activate();
    }
}

void WormTeam::RemoveWorm(Worm* worm)
{
    auto it = std::find_if(worms.begin(), worms.end(),
                           [worm](const auto& item) { return item.get() == worm; });
    if (it == worms.end())
    {
        return;
    }

    const auto index = static_cast<int>(it - worms.begin());
    const bool wasActive = index == activeWorm;

    (*it)->CleanUp();
    worms.erase(it);
    if (dieSound)
        dieSound->Play();

    if (worms.empty())
    {
        activeWorm = 0;
        return;
    }

    if (index < activeWorm)
    {
        --activeWorm;
    }

    ActiveWormCheck();
    if (wasActive)
    {
        worms[activeWorm]->Activate();
    }
}

void WormTeam::ChangeActiveWorm()
{
    if (worms.empty())
    {
        return;
    }

    if (worms.size() > activeWorm)
    {
        worms[activeWorm]->Disactivate();
    }

    activeWorm++;
    ActiveWormCheck();

    worms[activeWorm]->Activate();
}

EntityId WormTeam::GetActiveWorm()
{
    if (worms.empty())
    {
        throw std::logic_error("Cannot get active worm from an empty team");
    }
    ActiveWormCheck();
    return worms[activeWorm]->GetId();
}

int WormTeam::Size() const
{
    return static_cast<int>(worms.size());
}

void WormTeam::RenderHealthBars()
{
    for (const auto& worm : worms)
    {
        worm->Render();
    }
}

void WormTeam::Update()
{
    for (auto& worm : worms)
    {
        worm->Update(wormsToDelete);
    }
    for (auto worm : wormsToDelete)
    {
        RemoveWorm(worm);
    }
    wormsToDelete.clear();
}

void WormTeam::CleanUp()
{
    wormsToDelete.clear();

    for (auto& worm : worms)
    {
        worm->CleanUp();
    }

    worms.clear();
    dieSound = nullptr;
    initialized = false;
    activeWorm = 0;
}

void WormTeam::ActiveWormCheck()
{
    if (activeWorm >= worms.size())
    {
        activeWorm = 0;
    }
}
