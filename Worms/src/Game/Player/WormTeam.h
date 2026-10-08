#pragma once
#include "Game/Player/Worm.h"

class WormTeam
{
    friend class Worm;

public:
    void AddWorm(Worm* worm);
    void RemoveWorm(Worm* worm);
    void ChangeActiveWorm();
    EntityId GetActiveWorm();
    int Size() const;
    void Update();
    void RenderHealthBars();

    ~WormTeam();

private:
    void ActiveWormCheck();
    std::vector<Worm*> worms;
    std::vector<Worm*> wormsToDelete;
    Sound dieSound{"scream.wav"};
    int activeWorm = 0;
};