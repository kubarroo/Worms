#include "GameScene.h"

#include "Core/Audio/Music.h"
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/Physics/b2ColliderDraw.h"
#include "ECS/World.h"
#include "Game/Components.h"
#include "Game/Map/Map.h"
#include "Game/Player/WormManager.h"
#include "Game/Systems.h"
#include "Game/Weapon/WeaponManager.h"

#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Terminal/Terminal.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace
{
void ReportCleanupError(const char* operation, std::exception_ptr error) noexcept
{
    if (!error)
        return;
    try
    {
        try
        {
            std::rethrow_exception(error);
        }
        catch (const std::exception& exception)
        {
            Terminal::Get().Log(std::string(operation) + ": " + exception.what(),
                                ERROR);
        }
        catch (...)
        {
            Terminal::Get().Log(std::string(operation) + ": unknown exception", ERROR);
        }
    }
    catch (...)
    {
    } // Logging must not interrupt resource cleanup.
}

template <typename Cleanup>
void TryCleanup(const char* operation, Cleanup&& cleanup) noexcept
{
    try
    {
        cleanup();
    }
    catch (...)
    {
        ReportCleanupError(operation, std::current_exception());
    }
}
} // namespace

GameScene::GameScene(SDL_Renderer* renderer) : renderer(renderer)
{
    if (!renderer)
        throw std::invalid_argument("Scene requires a renderer");
}

GameScene::~GameScene()
{
    CleanUp();
}

void GameScene::Initialize()
{
    if (ownsRuntime)
        throw std::logic_error("Scene is already initialized");
    if (!GameObject::activeObjs.empty() || !GameObject::objsToAdd.empty() ||
        !GameObject::objsToDelete.empty())
        throw std::logic_error("Only one scene can use the global object collections");

    ownsRuntime = true;
    try
    {
        world = std::make_unique<World>(renderer);

        registerComponents();
        world->RegisterSystem<Movement>();
        world->RegisterSystem<PhysicsSynchronizer>();
        world->RegisterSystem<TargetSystem>(*world);
        world->RegisterSystem<ParticleUpdater>();
        auto camera = std::make_unique<Camera>();
        auto cameraPtr = camera.get();
        GameObject::activeObjs.emplace_back(std::move(camera));
        world->RegisterSystem<SpriteRenderer>(renderer, *cameraPtr);

        physicsWorld = std::make_unique<b2World>(b2Vec2(0, -9.811f));
        setUpDebugDraw(*cameraPtr);
        ColliderFactory::Get().Init(physicsWorld.get());
        weaponManager = std::make_unique<WeaponManager>(renderer, *cameraPtr);
        wormManager = std::make_unique<WormManager>(renderer, world.get(), physicsWorld.get(),
                                                    *cameraPtr, *weaponManager->GetWeapon());
        wormManager->CreateTeam(4);
        wormManager->CreateTeam(4);
        GameObject::activeObjs.emplace_back(std::make_unique<Map>(physicsWorld.get()));

        // Keep the camera's update/render order while owning it throughout initialization.
        auto cameraIt =
            std::find_if(GameObject::activeObjs.begin(), GameObject::activeObjs.end(),
                         [cameraPtr](const auto& object) { return object.get() == cameraPtr; });
        std::rotate(cameraIt, std::next(cameraIt), GameObject::activeObjs.end());

        for (auto& gameObject : GameObject::activeObjs)
            gameObject->Initialise(renderer, world.get());

        weaponManager->Initialise();

        cameraPtr->ChangeTarget(wormManager->GetActiveWormId());

        music = std::make_unique<Music>("Rick_Roll.ogg");
        music->Play();
        initialized = true;
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}

void GameScene::setUpDebugDraw(Camera& camera)
{
    b2DebugDraw = std::make_unique<b2ColliderDraw>(renderer, camera);
    physicsWorld->SetDebugDraw(b2DebugDraw.get());
    physicsWorld->SetContactListener(&ContactManager::Get());
}

void GameScene::registerComponents()
{
    world->RegisterComponent<Position>();
    world->RegisterComponent<Rotation>();
    world->RegisterComponent<Health>();
    world->RegisterComponent<Sprite>();
    world->RegisterComponent<Motion>();
    world->RegisterComponent<RigidBody>();
    world->RegisterComponent<Follow>();
    world->RegisterComponent<Scale>();
    world->RegisterComponent<Particle>();
}

void GameScene::Update()
{
    if (!initialized)
        return;
    ContactManager::Get().Update();

    world->Update();
    ContactManager::Get().RethrowPendingException();
    physicsWorld->Step(static_cast<float>(Time::deltaTime), 8, 3);
    ContactManager::Get().RethrowPendingException();
    wormManager->Update();
    weaponManager->Update();

    for (auto& ptr : GameObject::objsToAdd)
    {
        ptr->Initialise(renderer, world.get());
        GameObject::activeObjs.emplace_back(std::move(ptr));
    }
    GameObject::objsToAdd.clear();

    if (GameObject::objsToDelete.size() > 0)
        GameObject::activeObjs.erase(
            std::remove_if(GameObject::activeObjs.begin(), GameObject::activeObjs.end(),
                           [](std::unique_ptr<GameObject>& value)
                           {
                               bool found =
                                   std::find(GameObject::objsToDelete.begin(),
                                             GameObject::objsToDelete.end(),
                                             value.get()) != GameObject::objsToDelete.end();
                               if (found)
                                   value->CleanUp();
                               return found;
                           }),
            GameObject::activeObjs.end());

    GameObject::objsToDelete.clear();

    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Update();
    ContactManager::Get().RethrowPendingException();
}

void GameScene::Render()
{
    if (!initialized)
        return;
    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Render();

    world->Render();
    wormManager->RenderHealthBars();
}

void GameScene::RenderDebug()
{
    if (initialized)
        physicsWorld->DebugDraw();
}

void GameScene::CleanUp() noexcept
{
    if (!ownsRuntime)
        return;
    initialized = false;

    if (Mix_QuerySpec(nullptr, nullptr, nullptr))
    {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
    }

    if (physicsWorld)
    {
        physicsWorld->SetContactListener(nullptr);
        physicsWorld->SetDebugDraw(nullptr);
    }

    if (wormManager)
    {
        TryCleanup("Worm manager cleanup failed", [this] { wormManager->CleanUp(); });
    }

    for (auto& object : GameObject::activeObjs)
    {
        if (object)
        {
            TryCleanup("Active object cleanup failed", [&object] { object->CleanUp(); });
        }
    }
    for (auto& object : GameObject::objsToAdd)
    {
        if (object)
        {
            TryCleanup("Pending object cleanup failed", [&object] { object->CleanUp(); });
        }
    }

    ReportCleanupError("Collision callback failed during cleanup",
                       ContactManager::Get().TakePendingException());
    ContactManager::Get().ClearAll();

    wormManager.reset();
    GameObject::objsToDelete.clear();
    GameObject::objsToAdd.clear();
    GameObject::activeObjs.clear();

    weaponManager.reset();
    music.reset();
    b2DebugDraw.reset();
    ColliderFactory::Get().Init(nullptr);
    physicsWorld.reset();
    world.reset();

    ownsRuntime = false;
}
