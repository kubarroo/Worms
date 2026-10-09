#pragma once
#include "Game/Player/Worm.h"
#include <memory>

class WormTeam
{
    friend class Worm;

public:
    using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;

    explicit WormTeam(TexturePtr texture) : healthBarTexture(std::move(texture)) {}

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
    Sound dieSound{"scream.wav"};
    int activeWorm = 0;
};