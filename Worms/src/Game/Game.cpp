#include "Game.h"
#include "GameScene.h"

Game::Game() = default;

Game::~Game()
{
    Clean();
}

void Game::InitWindow(const std::string& title, const int width, const int height)
{
    App::InitWindow(title, width, height);
    scene = std::make_unique<GameScene>(renderer);
    scene->Initialize();
}

void Game::Update()
{
    App::Update();
    if (scene)
        scene->Update();
}

void Game::Render()
{
    if (!scene)
        return;
    scene->Render();
    if (ShouldRenderColliders())
        scene->RenderDebug();
}

void Game::Clean()
{
    scene.reset();
    App::Clean();
}
