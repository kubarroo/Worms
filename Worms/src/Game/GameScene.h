#pragma once
#include <deque>
#include <memory>
#include <vector>

struct SDL_Renderer;
class World;
class b2World;
class b2ColliderDraw;
class Camera;
class WormManager;
class WeaponManager;
class Music;
class GameObject;

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
    void QueueAdd(std::unique_ptr<GameObject> object);
    void RequestDestroy(GameObject& object);
    // Immediate initialization is allowed only outside frame processing.
    GameObject& AddObject(std::unique_ptr<GameObject> object);
    // Reports cleanup errors and continues releasing the remaining scene resources.
    void CleanUp() noexcept;

private:
    friend struct GameTestAccess;
    void setUpDebugDraw(Camera& camera);
    void registerComponents();
    GameObject& ActivateObject(std::unique_ptr<GameObject> object);
    void ProcessPendingAdds();
    void ProcessPendingRemovals();
    void ValidateObject(const GameObject& object) const;

    SDL_Renderer* renderer;
    bool ownsRuntime = false;
    bool initialized = false;
    bool processingFrame = false;
    bool cleaningUp = false;
    GameObject* initializingObject = nullptr;
    std::unique_ptr<GameObject> failedStartupObject;
    std::vector<std::unique_ptr<GameObject>> activeObjects;
    std::deque<std::unique_ptr<GameObject>> pendingAdds;
    std::deque<GameObject*> pendingRemovals;
    std::unique_ptr<World> world;
    std::unique_ptr<b2World> physicsWorld;
    std::unique_ptr<b2ColliderDraw> b2DebugDraw;
    std::unique_ptr<WormManager> wormManager;
    std::unique_ptr<WeaponManager> weaponManager;
    std::unique_ptr<Music> music;
};
