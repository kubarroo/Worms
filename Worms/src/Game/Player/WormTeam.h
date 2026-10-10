#pragma once
#include "Game/Player/Worm.h"
#include <memory>

class WormTeam
{
    friend class Worm;

public:
    using TexturePtr = Sdl::TexturePtr;

    explicit WormTeam(TexturePtr texture) : healthBarTexture(std::move(texture)) {}

    void Initialise();
    void AddWorm(std::unique_ptr<Worm> worm);
    void RemoveWorm(Worm* worm);
    void ChangeActiveWorm();
    EntityId GetActiveWorm();
    SDL_Texture* GetHealthBarTexture() const
    {
        return healthBarTexture.get();
    }
    int Size() const;
    void Update();
    void RenderHealthBars();
    void CleanUp();

    ~WormTeam() = default;

private:
    void ActiveWormCheck();
    TexturePtr healthBarTexture;
    std::vector<std::unique_ptr<Worm>> worms;
    std::vector<Worm*> wormsToDelete;
    std::unique_ptr<Sound> dieSound;
    bool initialized = false;
    int activeWorm = 0;
};
