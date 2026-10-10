#include "GameScene.h"

#include "Core/Audio/Music.h"
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/Input.h"
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
class ScopedFrame
{
public:
    explicit ScopedFrame(bool& flag) : flag(flag)
    {
        flag = true;
    }
    ~ScopedFrame()
    {
        flag = false;
    }

private:
    bool& flag;
};

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
            Terminal::Get().Log(std::string(operation) + ": " + exception.what(), ERROR);
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

template <typename Cleanup> void TryCleanup(const char* operation, Cleanup&& cleanup) noexcept
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

    ownsRuntime = true;
    try
    {
        world = std::make_unique<World>(renderer);

        registerComponents();
        world->RegisterSystem<Movement>();
        world->RegisterSystem<PhysicsSynchronizer>();
        world->RegisterSystem<TargetSystem>(*world);
        world->RegisterSystem<ParticleUpdater>();
        physicsWorld = std::make_unique<b2World>(b2Vec2(0, -9.811f));
        contacts = std::make_unique<ContactManager>();
        colliders = std::make_unique<ColliderFactory>(*physicsWorld, *contacts);
        context = std::make_unique<SceneContext>(renderer, *world, *physicsWorld, *this, *colliders,
                                                 *contacts);
        auto camera = std::make_unique<Camera>();
        auto cameraPtr = camera.get();
        this->camera = cameraPtr;
        QueueAdd(std::move(camera));
        world->RegisterSystem<SpriteRenderer>(renderer, *cameraPtr);

        setUpDebugDraw(*cameraPtr);
        auto weapon = std::make_unique<Weapon>(*cameraPtr);
        auto* weaponPtr = weapon.get();
        QueueAdd(std::move(weapon));
        weaponManager = std::make_unique<WeaponManager>(*renderer, *weaponPtr);
        wormManager = std::make_unique<WormManager>(*context, *cameraPtr, *weaponPtr);
        wormManager->Initialise();
        wormManager->CreateTeam(4);
        wormManager->CreateTeam(4);
        QueueAdd(std::make_unique<Map>());

        // Keep the camera's update/render order while owning it throughout initialization.
        auto cameraIt =
            std::find_if(pendingAdds.begin(), pendingAdds.end(),
                         [cameraPtr](const auto& object) { return object.get() == cameraPtr; });
        std::rotate(cameraIt, std::next(cameraIt), pendingAdds.end());

        ProcessPendingAdds();

        weaponManager->Initialise();

        cameraPtr->ChangeTarget(wormManager->GetActiveWormId());

        music = std::make_unique<Music>("Rick_Roll.ogg");
        music->Play();
        Input::Get().Reset();
        Time::ResetFrameClock();
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
    physicsWorld->SetContactListener(contacts.get());
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

const SceneContext& GameScene::Context() const
{
    if (!ownsRuntime || cleaningUp || !context)
        throw std::logic_error("Scene context is unavailable");
    return *context;
}

void GameScene::ValidateObject(const GameObject& object) const
{
    if (!ownsRuntime || cleaningUp)
        throw std::logic_error("Scene is not accepting objects");
    if (object.context && &object.context->objects != this)
        throw std::invalid_argument("Object belongs to another scene");
    if (object.HasEntity() && object.world != world.get())
        throw std::invalid_argument("Object belongs to another ECS world");
    if (object.context &&
        (&object.context->physics != physicsWorld.get() || object.context->renderer != renderer ||
         &object.context->colliders != colliders.get() ||
         &object.context->contacts != contacts.get()))
        throw std::invalid_argument("Object uses different scene services");
}

void GameScene::QueueAdd(std::unique_ptr<GameObject> object)
{
    if (!object)
        throw std::invalid_argument("Cannot add a null object");
    try
    {
        ValidateObject(*object);
        if (object->HasEntity())
            throw std::invalid_argument("QueueAdd requires an uninitialized object");
    }
    catch (...)
    {
        TryCleanup("Rejected queued object cleanup", [&object] { object->CleanUp(); });
        throw;
    }
    pendingAdds.emplace_back(std::move(object));
}

GameObject& GameScene::AddObject(std::unique_ptr<GameObject> object)
{
    if (processingFrame || initializingObject)
    {
        if (object)
            TryCleanup("Rejected immediate object cleanup", [&object] { object->CleanUp(); });
        throw std::logic_error("Use QueueAdd during frame processing or initialization");
    }
    return ActivateObject(std::move(object));
}

GameObject& GameScene::ActivateObject(std::unique_ptr<GameObject> object)
{
    if (!object)
        throw std::invalid_argument("Cannot add a null object");
    try
    {
        ValidateObject(*object);
    }
    catch (...)
    {
        TryCleanup("Rejected object cleanup", [&object] { object->CleanUp(); });
        throw;
    }
    initializingObject = object.get();
    try
    {
        if (activeObjects.size() == activeObjects.capacity())
            activeObjects.reserve(std::max(activeObjects.size() + 1, activeObjects.capacity() * 2));
        if (!object->HasEntity())
            object->Initialise(*context);
        auto& result = *object;
        activeObjects.emplace_back(std::move(object));
        initializingObject = nullptr;
        return result;
    }
    catch (...)
    {
        std::erase(pendingRemovals, initializingObject);
        initializingObject = nullptr;
        TryCleanup("Failed object initialization cleanup", [&object] { object->CleanUp(); });
        // Startup managers may still reference this object while the scene rolls back.
        if (!initialized)
            failedStartupObject = std::move(object);
        throw;
    }
}

void GameScene::RequestDestroy(GameObject& object)
{
    if (!ownsRuntime || cleaningUp)
        throw std::logic_error("Scene is not accepting removal requests");
    const auto owns = [&object](const auto& item) { return item.get() == &object; };
    if (initializingObject != &object &&
        std::none_of(activeObjects.begin(), activeObjects.end(), owns) &&
        std::none_of(pendingAdds.begin(), pendingAdds.end(), owns))
        throw std::invalid_argument("Object is not owned by this scene");
    if (std::find(pendingRemovals.begin(), pendingRemovals.end(), &object) == pendingRemovals.end())
        pendingRemovals.push_back(&object);
}

void GameScene::ProcessPendingAdds()
{
    // Process only this batch; additions from initialization wait for the next frame.
    const auto count = pendingAdds.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        auto object = std::move(pendingAdds.front());
        pendingAdds.pop_front();
        auto removal = std::find(pendingRemovals.begin(), pendingRemovals.end(), object.get());
        if (removal != pendingRemovals.end())
        {
            pendingRemovals.erase(removal);
            TryCleanup("Cancelled object cleanup failed", [&object] { object->CleanUp(); });
            continue;
        }
        ActivateObject(std::move(object));
    }
}

void GameScene::ProcessPendingRemovals()
{
    const auto count = pendingRemovals.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        auto* object = pendingRemovals.front();
        auto found = std::find_if(activeObjects.begin(), activeObjects.end(),
                                  [object](const auto& item) { return item.get() == object; });
        if (found == activeObjects.end())
        {
            // Objects queued by an initializer may still await the next addition batch.
            std::rotate(pendingRemovals.begin(), std::next(pendingRemovals.begin()),
                        pendingRemovals.end());
            continue;
        }
        // Keep ownership and the removal request if cleanup throws; retry next frame.
        (*found)->CleanUp();
        pendingRemovals.pop_front();
        activeObjects.erase(found);
    }
}

void GameScene::Update()
{
    if (!initialized)
        return;
    if (processingFrame)
        throw std::logic_error("Scene update is already in progress");
    ScopedFrame frame(processingFrame);
    contacts->Update();

    world->Update();
    contacts->RethrowPendingException();
    physicsWorld->Step(static_cast<float>(Time::deltaTime), 8, 3);
    contacts->RethrowPendingException();
    wormManager->Update();
    weaponManager->Update();

    ProcessPendingAdds();
    ProcessPendingRemovals();

    for (auto& gameObject : activeObjects)
    {
        gameObject->Update();
        if (gameObject.get() == camera && camera->ConsumeTargetLost())
            wormManager->OnCameraTargetLost();
    }
    contacts->RethrowPendingException();
}

void GameScene::Render()
{
    if (!initialized)
        return;
    if (processingFrame)
        throw std::logic_error("Scene frame is already in progress");
    ScopedFrame frame(processingFrame);
    for (auto& gameObject : activeObjects)
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
    if (!ownsRuntime || cleaningUp)
        return;
    initialized = false;
    cleaningUp = true;

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

    if (failedStartupObject)
    {
        TryCleanup("Failed startup object cleanup",
                   [this] { failedStartupObject->CleanUp(); });
    }

    for (auto& object : activeObjects)
    {
        if (object)
        {
            TryCleanup("Active object cleanup failed", [&object] { object->CleanUp(); });
        }
    }
    for (auto& object : pendingAdds)
    {
        if (object)
        {
            TryCleanup("Pending object cleanup failed", [&object] { object->CleanUp(); });
        }
    }

    if (contacts)
    {
        ReportCleanupError("Collision callback failed during cleanup",
                           contacts->TakePendingException());
        contacts->ClearAll();
    }

    wormManager.reset();
    camera = nullptr;
    pendingRemovals.clear();
    pendingAdds.clear();
    activeObjects.clear();

    weaponManager.reset();
    music.reset();
    b2DebugDraw.reset();
    failedStartupObject.reset();
    context.reset();
    colliders.reset();
    physicsWorld.reset();
    contacts.reset();
    world.reset();

    ownsRuntime = false;
    cleaningUp = false;
}
