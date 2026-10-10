#pragma once
#include <memory>

struct SDL_Renderer;
class World;
class b2World;
class b2ColliderDraw;
class Camera;
class WormManager;
class WeaponManager;
class Music;

class GameScene
{
public:
    explicit GameScene(SDL_Renderer* renderer);
    ~GameScene();
    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;
    GameScene(GameScene&&) = delete;
    GameScene& operator=(GameScene&&) = delete;

    void Initialize();
    void Update();
    void Render();
    void RenderDebug();
    // Reports cleanup errors and continues releasing the remaining scene resources.
    void CleanUp() noexcept;

private:
    friend struct GameTestAccess;
    void setUpDebugDraw(Camera& camera);
    void registerComponents();

    SDL_Renderer* renderer;
    bool ownsRuntime = false;
    bool initialized = false;
    std::unique_ptr<World> world;
    std::unique_ptr<b2World> physicsWorld;
    std::unique_ptr<b2ColliderDraw> b2DebugDraw;
    std::unique_ptr<WormManager> wormManager;
    std::unique_ptr<WeaponManager> weaponManager;
    std::unique_ptr<Music> music;
};
