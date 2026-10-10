#pragma once
#include "Core/SceneContext.h"
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

class GameScene : public ObjectCommands
{
public:
    GameScene(SDL_Renderer* renderer, ResourceManager& resources);
    ~GameScene() override;
    GameScene(const GameScene&) = delete;
    GameScene& operator=(const GameScene&) = delete;
    GameScene(GameScene&&) = delete;
    GameScene& operator=(GameScene&&) = delete;

    void Initialize();
    void Update();
    void Render();
    void RenderDebug();
    const SceneContext& Context() const;
    void QueueAdd(std::unique_ptr<GameObject> object) override;
    void RequestDestroy(GameObject& object) override;
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
    ResourceManager& resources;
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
    std::unique_ptr<ContactManager> contacts;
    std::unique_ptr<ColliderFactory> colliders;
    Camera* camera = nullptr;
    std::unique_ptr<SceneContext> context;
    std::unique_ptr<b2ColliderDraw> b2DebugDraw;
    std::unique_ptr<WormManager> wormManager;
    std::unique_ptr<WeaponManager> weaponManager;
    std::unique_ptr<Music> music;
};
