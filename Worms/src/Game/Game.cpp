#include "Game.h"

#include "Core/Input.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <imgui_impl_sdlrenderer2.h>
#include <algorithm>
#include <iterator>

namespace
{
void ReportContactErrorDuringCleanup() noexcept
{
    auto error = ContactManager::Get().TakePendingException();
    if (!error) return;
    try
    {
        try { std::rethrow_exception(error); }
        catch (const std::exception& exception)
        {
            Terminal::Get().Log(std::string("Collision callback failed: ") + exception.what(), ERROR);
        }
        catch (...)
        {
            Terminal::Get().Log("Collision callback failed with an unknown exception", ERROR);
        }
    }
    catch (...) {} // Logging must not interrupt resource cleanup.
}
}

void Game::InitWindow(const std::string& title, const int width, const int height)
{
    App::InitWindow(title, width, height);

    world = std::make_unique<World>(renderer);

    registerComponents();
    world->RegisterSystem<Movement>();
    world->RegisterSystem<PhysicsSynchronizer>();
    world->RegisterSystem<TargetSystem>();
    world->RegisterSystem<ParticleUpdater>();
    auto camera = std::make_unique<Camera>();
    auto cameraPtr = camera.get();
    GameObject::activeObjs.emplace_back(std::move(camera));
    world->RegisterSystem<SpriteRenderer>(renderer, *cameraPtr);

    physicsWorld = std::make_unique<b2World>(b2Vec2(0, -9.811f));
    setUpDebugDraw(*cameraPtr);
    ColliderFactory::Get().Init(physicsWorld.get());
    weaponManager = std::make_unique<WeaponManager>(renderer, *cameraPtr);
    wormManager = std::make_unique<WormManager>(renderer, world.get(), physicsWorld.get(), *cameraPtr,
                                                *weaponManager->GetWeapon());
    wormManager->CreateTeam(4);
    wormManager->CreateTeam(4);
    GameObject::activeObjs.emplace_back(std::make_unique<Map>(physicsWorld.get()));

    // Keep the camera's update/render order while owning it throughout initialization.
    auto cameraIt = std::find_if(GameObject::activeObjs.begin(), GameObject::activeObjs.end(),
                                [cameraPtr](const auto& object) { return object.get() == cameraPtr; });
    std::rotate(cameraIt, std::next(cameraIt), GameObject::activeObjs.end());

    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Initialise(renderer, world.get());

    weaponManager->Initialise();

    cameraPtr->ChangeTarget(wormManager->GetActiveWormId());

    music = std::make_unique<Music>("Rick_Roll.ogg");
    music->Play();
}

void Game::setUpDebugDraw(Camera& camera)
{
    b2DebugDraw = std::make_unique<b2ColliderDraw>(renderer, camera);
    physicsWorld->SetDebugDraw(b2DebugDraw.get());
    physicsWorld->SetContactListener(&ContactManager::Get());
}

void Game::registerComponents()
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

void Game::Update()
{
    App::Update();

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

void Game::Render()
{
    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Render();

    world->Render();
    wormManager->RenderHealthBars();
}

void Game::Clean()
{
    if (audioOpened)
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
        wormManager->CleanUp();
    }

    for (auto& object : GameObject::activeObjs)
    {
        if (object)
        {
            object->CleanUp();
        }
    }
    for (auto& object : GameObject::objsToAdd)
    {
        if (object)
        {
            object->CleanUp();
        }
    }

    ReportContactErrorDuringCleanup();
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

    App::Clean();
}
