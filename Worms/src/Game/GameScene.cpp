#include "GameScene.h"

#include "Core/Audio/Music.h"
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/Input.h"
#include "Core/ResourceManager.h"
#include "Core/Physics/b2ColliderDraw.h"
#include "Game/Components.h"
#include "Game/RenderOrder.h"
#include "Game/Map/Map.h"
#include "Game/Player/WormManager.h"
#include "Game/Systems.h"
#include "Game/Weapon/WeaponManager.h"

#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Terminal/Terminal.h"
#include <SDL3/SDL.h>
#include "Core/Renderer2D.h"
#include "Core/Audio/Audio.h"
#include <algorithm>
#include <iterator>
#include <stdexcept>

namespace
{
class ScopedFrame
{
public:
    ScopedFrame(bool& flag, bool& cleanupRequested, GameScene& scene)
        : flag(flag), cleanupRequested(cleanupRequested), scene(scene)
    {
        flag = true;
    }
    ~ScopedFrame()
    {
        flag = false;
        if (cleanupRequested)
            scene.CleanUp();
    }

private:
    bool& flag;
    bool& cleanupRequested;
    GameScene& scene;
};

class ScopedObjectCleanup
{
public:
    ScopedObjectCleanup(unsigned& depth, bool& requested, GameScene& scene)
        : depth(depth), requested(requested), scene(scene) { ++depth; }
    ~ScopedObjectCleanup()
    {
        if (--depth == 0 && requested) scene.CleanUp();
    }
private:
    unsigned& depth;
    bool& requested;
    GameScene& scene;
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

template <typename Cleanup> bool TryCleanup(const char* operation, Cleanup&& cleanup) noexcept
{
    try
    {
        cleanup();
        return true;
    }
    catch (...)
    {
        ReportCleanupError(operation, std::current_exception());
        return false;
    }
}
} // namespace

GameScene::GameScene(SDL_Renderer* renderer, ResourceManager& resources,
                     Renderer2D& rendering, Audio& audio, Input& input)
    : renderer(renderer), resources(resources), rendering(rendering), audio(audio), input(input)
{
    if (!renderer)
        throw std::invalid_argument("Scene requires a renderer");
    if (resources.Renderer() != renderer || rendering.Native() != renderer)
        throw std::invalid_argument("Scene and resource manager require the same renderer");
    if (resources.Mixer() != audio.Native())
        throw std::invalid_argument("Scene and resource manager require the same mixer");
}

GameScene::~GameScene()
{
    // An owner must not destroy a scene from one of its executing callbacks.
    if (processingFrame || initializingObject || cleaningUp || cleanupDepth ||
        (physicsWorld && physicsWorld->IsLocked()))
        std::terminate();
    CleanUp();
}

void GameScene::Initialize()
{
    if (ownsRuntime)
        throw std::logic_error("Scene is already initialized");

    ownsRuntime = true;
    ScopedFrame frame(processingFrame, cleanupRequested, *this);
    try
    {
        physicsWorld = std::make_unique<b2World>(b2Vec2(0, -9.811f));
        contacts = std::make_unique<ContactManager>(registry);
        colliders = std::make_unique<ColliderFactory>(*physicsWorld, *contacts);
        context = std::make_unique<SceneContext>(renderer, registry, *physicsWorld, *this, *colliders,
                                                 *contacts, resources, rendering, audio, input);
        auto camera = std::make_unique<Camera>();
        auto cameraPtr = camera.get();
        this->camera = cameraPtr;
        QueueAdd(std::move(camera));

        setUpDebugDraw(*cameraPtr);
        auto weapon = std::make_unique<Weapon>(*cameraPtr);
        auto* weaponPtr = weapon.get();
        QueueAdd(std::move(weapon));
        weaponManager = std::make_unique<WeaponManager>(resources, *weaponPtr, input);
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

        cameraPtr->ChangeTarget(wormManager->GetActiveWormHandle());

        music = &resources.GetMusic("Rick_Roll.ogg");
        audio.Play(*music);
        input.Reset();
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
    b2DebugDraw = std::make_unique<b2ColliderDraw>(rendering, camera);
    physicsWorld->SetDebugDraw(b2DebugDraw.get());
    physicsWorld->SetContactListener(contacts.get());
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
    if (object.context &&
        (&object.context->registry != &registry ||
         &object.context->physics != physicsWorld.get() || object.context->renderer != renderer ||
         &object.context->colliders != colliders.get() ||
         &object.context->contacts != contacts.get() || &object.context->resources != &resources ||
         &object.context->rendering != &rendering || &object.context->audio != &audio ||
         &object.context->input != &input))
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
        CleanupRejected(std::move(object));
        throw;
    }
    pendingAdds.emplace_back(std::move(object));
}

GameObject& GameScene::AddObject(std::unique_ptr<GameObject> object)
{
    if (processingFrame || initializingObject || cleanupDepth ||
        (physicsWorld && physicsWorld->IsLocked()))
    {
        if (object)
            CleanupRejected(std::move(object));
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
        CleanupRejected(std::move(object));
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
        const auto initializationError = std::current_exception();
        ScopedObjectCleanup rollback(cleanupDepth, cleanupRequested, *this);
        std::erase(pendingRemovals, initializingObject);
        initializingObject = nullptr;
        const bool cleaned = TryCleanup("Failed object initialization cleanup",
                                        [&object] { object->CleanUp(); });
        // Startup managers may still reference this object while the scene rolls back.
        if (!initialized)
            failedStartupObject = std::move(object);
        else if (!cleaned)
            rejectedObjects.emplace_back(std::move(object));
        else
            object.reset();
        std::rethrow_exception(initializationError);
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
    RequireUnlockedPhysics();
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
        if (cleanupRequested) return;
    }
}

void GameScene::ProcessPendingRemovals()
{
    RequireUnlockedPhysics();
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
    RequireUnlockedPhysics();
    if (processingFrame || cleanupDepth)
        throw std::logic_error("Scene update is already in progress");
    if (cleanupRequested && !processingFrame)
    {
        CleanUp();
        return;
    }
    if (!initialized)
        return;
    ScopedFrame frame(processingFrame, cleanupRequested, *this);
    ProcessRejectedObjects();
    contacts->Update();

    Movement::Update(registry);
    PhysicsSynchronizer::Update(registry);
    TargetSystem::Update(registry);
    ParticleUpdater::Update(registry);
    contacts->RethrowPendingException();
    if (cleanupRequested) return;
    physicsWorld->Step(static_cast<float>(Time::deltaTime), 8, 3);
    contacts->RethrowPendingException();
    if (cleanupRequested) return;
    wormManager->Update();
    weaponManager->Update();

    ProcessPendingAdds();
    ProcessPendingRemovals();
    ProcessRejectedObjects();
    if (cleanupRequested) return;

    for (auto& gameObject : activeObjects)
    {
        gameObject->Update();
        if (cleanupRequested) return;
        if (gameObject.get() == camera && camera->ConsumeTargetLost())
            wormManager->OnCameraTargetLost();
    }
    contacts->RethrowPendingException();
}

void GameScene::Render()
{
    RequireUnlockedPhysics();
    if (processingFrame || cleanupDepth)
        throw std::logic_error("Scene frame is already in progress");
    if (cleanupRequested && !processingFrame)
    {
        CleanUp();
        return;
    }
    if (!initialized)
        return;
    ScopedFrame frame(processingFrame, cleanupRequested, *this);
    for (auto& gameObject : activeObjects)
    {
        gameObject->Render();
        if (cleanupRequested) return;
    }

    SpriteRenderer::Render(registry, rendering, *camera);
    wormManager->RenderHealthBars();
}

void GameScene::RenderDebug()
{
    RequireUnlockedPhysics();
    if (processingFrame || cleanupDepth)
        throw std::logic_error("Scene frame is already in progress");
    ScopedFrame frame(processingFrame, cleanupRequested, *this);
    if (initialized)
        physicsWorld->DebugDraw();
}

void GameScene::CleanUp() noexcept
{
    if (!ownsRuntime || cleaningUp)
        return;
    if (processingFrame || initializingObject || cleanupDepth ||
        (physicsWorld && physicsWorld->IsLocked()))
    {
        cleanupRequested = true;
        return;
    }
    cleanupRequested = false;
    initialized = false;
    cleaningUp = true;

    audio.StopAll();

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

    // Indexing remains valid when a cleanup callback retires another object.
    for (std::size_t index = 0; index < rejectedObjects.size(); ++index)
        TryCleanup("Rejected object cleanup failed", [this, index]
                   { rejectedObjects[index]->CleanUp(); });

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
    music = nullptr;
    b2DebugDraw.reset();
    failedStartupObject.reset();
    context.reset();
    colliders.reset();
    physicsWorld.reset();
    rejectedObjects.clear();
    contacts.reset();
    registry.clear();
    registry.ctx().erase<RenderSequence>();

    ownsRuntime = false;
    cleaningUp = false;
}

void GameScene::RequireUnlockedPhysics() const
{
    if (physicsWorld && physicsWorld->IsLocked())
        throw std::logic_error("Scene structural operations require an unlocked physics world");
}

void GameScene::CleanupRejected(std::unique_ptr<GameObject> object)
{
    // Route foreign initialized objects to their owner, preserving borrowed fixture metadata.
    auto* owner = object->context ? dynamic_cast<GameScene*>(&object->context->objects) : this;
    if (!owner) owner = this;
    if (owner->processingFrame || owner->initializingObject || owner->cleaningUp || owner->cleanupDepth ||
        (owner->physicsWorld && owner->physicsWorld->IsLocked()) ||
        (object->context && object->context->physics.IsLocked()))
        owner->rejectedObjects.emplace_back(std::move(object));
    else
    {
        ScopedObjectCleanup cleanup(owner->cleanupDepth, owner->cleanupRequested, *owner);
        if (!TryCleanup("Rejected object cleanup", [&object] { object->CleanUp(); }))
            owner->rejectedObjects.emplace_back(std::move(object));
        else
            object.reset();
    }
}

void GameScene::RetireObject(std::unique_ptr<GameObject> object)
{
    if (object) CleanupRejected(std::move(object));
}

void GameScene::ProcessRejectedObjects()
{
    RequireUnlockedPhysics();
    const auto count = rejectedObjects.size();
    for (std::size_t index = 0; index < count; ++index)
    {
        // Retain ownership on failure, just like ordinary removal requests.
        rejectedObjects.front()->CleanUp();
        rejectedObjects.pop_front();
    }
}
