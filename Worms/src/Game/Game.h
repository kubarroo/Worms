#pragma once
#include "Core/Audio/Music.h"
#include "Core/Camera/Camera.h"
#include "Core/Initialization/App.h"
#include "Game/Weapon/WeaponManager.h"
class Game : public App
{
public:
    void InitWindow(const std::string& title, const int width, const int height) final;
    void Update() final;
    // void HandleEvents() final;
    void Render() final;
    void Clean() final;

private:
    void setUpDebugDraw(Camera& camera);
    void registerComponents();
    std::unique_ptr<WormManager> wormManager;
    std::unique_ptr<WeaponManager> weaponManager;
    std::unique_ptr<Music> music;
};
