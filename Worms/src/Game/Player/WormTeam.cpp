#include "Game/Player/WormTeam.h"

void WormTeam::AddWorm(Worm* worm)
{
    worms.push_back(worm);
    if (worms.size() == 1)
        worms[0]->Activate();
}

void WormTeam::RemoveWorm(Worm* worm)
{
    dieSound.Play();

    bool needToUpdate = false;
    ActiveWormCheck();
    if (worm == worms[activeWorm])
        needToUpdate = true;

    worms.erase(std::remove_if(worms.begin(), worms.end(),
                               [worm](Worm* current) { return worm == current; }));

    if (needToUpdate)
        ChangeActiveWorm();
}

void WormTeam::ChangeActiveWorm()
{
    if (worms.empty())
        return;

    if (worms.size() > activeWorm)
        worms[activeWorm]->Disactivate();

    activeWorm++;
    ActiveWormCheck();

    worms[activeWorm]->Activate();
}

EntityId WormTeam::GetActiveWorm()
{
    ActiveWormCheck();
    return worms[activeWorm]->GetId();
}

int WormTeam::Size() const
{
    return worms.size();
}

void WormTeam::RenderHealthBars()
{
    for (auto worm : worms)
    {
        worm->Render();
    }
}

void WormTeam::Update()
{
    for (auto& worm : worms)
        worm->Update(wormsToDelete);
    for (auto worm : wormsToDelete)
    {
        worm->CleanUp();
        RemoveWorm(worm);
    }
    wormsToDelete.clear();
}

WormTeam::~WormTeam()
{
    for (auto& worm : worms)
        delete worm;
    worms.clear();
}

void WormTeam::ActiveWormCheck()
{
    if (activeWorm >= worms.size())
    {
        activeWorm = 0;
    }
}
