#include "Game.h"

#include "Core/Input.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <imgui_impl_sdlrenderer2.h>


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
    world->RegisterSystem<SpriteRenderer>(renderer, *camera);

    physicsWorld = std::make_unique<b2World>(b2Vec2(0, -9.811f));
    setUpDebugDraw(camera);
    ColliderFactory::Get().Init(physicsWorld.get());
    weaponManager = std::make_unique<WeaponManager>(renderer, *camera);
    wormManager = std::make_unique<WormManager>(renderer, world.get(), physicsWorld.get(), *camera,
                                                *weaponManager->GetWeapon());
    wormManager->CreateTeam(4);
    wormManager->CreateTeam(4);
    GameObject::activeObjs.emplace_back(std::make_unique<Map>(physicsWorld.get()));
    GameObject::activeObjs.emplace_back(std::move(camera));

    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Initialise(renderer, world.get());

    weaponManager->Initialise();

    cameraPtr->ChangeTarget(wormManager->GetActiveWormId());

    music = std::make_unique<Music>("Rick_Roll.ogg");
    music->Play();
}

void Game::setUpDebugDraw(std::unique_ptr<Camera, std::default_delete<Camera>>& camera)
{
    b2DebugDraw = std::make_unique<b2ColliderDraw>(renderer, *camera);
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
    physicsWorld->Step(static_cast<float>(Time::deltaTime), 8, 3);
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
}

void Game::Render()
{
    for (auto& gameObject : GameObject::activeObjs)
        gameObject->Render();

    world->Render();
    wormManager->RenderHealthBars();
}
