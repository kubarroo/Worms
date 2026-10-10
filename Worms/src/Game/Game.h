#pragma once
#include "Core/Initialization/App.h"
#include <memory>

class GameScene;

class Game : public App
{
public:
    Game();
    ~Game() override;
    void InitWindow(const std::string& title, const int width, const int height) final;
    void Update() final;
    void Render() final;
    void Clean() noexcept final;

private:
    friend struct GameTestAccess;
    std::unique_ptr<GameScene> scene;
};
