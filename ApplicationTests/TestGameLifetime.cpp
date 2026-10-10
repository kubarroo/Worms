#include "Core/Audio/Music.h"
#include "Core/Audio/Sound.h"
#include "Core/ParticleSystem.h"
#include "Core/Time.h"
#include "Core/Physics/ColliderFactory.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Game.h"
#include "Game/GameScene.h"
#include "Game/Map/Map.h"
#include "Game/Player/WormManager.h"
#include "Game/Player/WormTeam.h"
#include "Game/Weapon/Projectile.h"
#include <SDL_mixer.h>
#include <box2d/b2_circle_shape.h>
#include <gtest/gtest.h>
#include <memory>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Sound>);
static_assert(!std::is_copy_assignable_v<Sound>);
static_assert(!std::is_copy_constructible_v<Music>);
static_assert(!std::is_copy_assignable_v<Music>);
static_assert(!std::is_copy_constructible_v<GameScene>);
static_assert(!std::is_move_constructible_v<GameScene>);
static_assert(noexcept(std::declval<GameScene&>().CleanUp()));

struct GameTestAccess
{
    static World& Registry(Game& game) { return *game.scene->world; }
    static b2World& Physics(Game& game) { return *game.scene->physicsWorld; }
    static bool HasScene(const Game& game) { return static_cast<bool>(game.scene); }
    static void ResetScene(Game& game) { game.scene.reset(); }
    static World& Registry(GameScene& scene) { return *scene.world; }
    static b2World& Physics(GameScene& scene) { return *scene.physicsWorld; }
    static bool HasResources(const GameScene& scene)
    {
        return scene.world || scene.physicsWorld || scene.b2DebugDraw ||
               scene.wormManager || scene.weaponManager || scene.music || scene.ownsRuntime;
    }
};

struct MapTestAccess
{
    static SDL_Surface* Surface(Map& map) { return map.physTex->surface.get(); }
    static bool HasResources(const Map& map)
    {
        return map.mapTexture || map.physTex || map.mapBody;
    }
    static void RequestRebuild(Map& map)
    {
        map.destroyed = true;
        map.destructionRadius = 0;
        map.bulltetPos = {0, 0};
    }
    static bool HasPendingRebuild(const Map& map) { return map.destroyed; }
    static int OwnedResources(const Map& map)
    {
        return static_cast<bool>(map.mapTexture) +
               static_cast<bool>(map.physTex && map.physTex->surface);
    }
    static void RequestDeformation(Map& map, Position point, float radius)
    {
        map.destroyed = true;
        map.bulltetPos = point;
        map.destructionRadius = radius;
    }
    static void UpdateWithoutRenderer(Map& map)
    {
        auto* renderer = map.renderer;
        map.renderer = nullptr;
        try { map.Update(); }
        catch (...) { map.renderer = renderer; throw; }
        map.renderer = renderer;
    }
};

struct WeaponTestAccess
{
    static int OwnedResources(const Weapon& weapon) { return static_cast<bool>(weapon.powerBar); }
    static bool HasParent(const Weapon& weapon) { return weapon.parentId.has_value(); }
    static float Charge(const Weapon& weapon) { return weapon.force; }
    static void SetCharge(Weapon& weapon) { weapon.force = 0.5f; }
};

struct ParticleSystemTestAccess
{
    static int OwnedResources(const ParticleSystem& particles)
    {
        return static_cast<bool>(particles.texture);
    }
    static std::size_t ParticleCount(const ParticleSystem& particles)
    {
        return particles.particles.size();
    }
};

namespace
{
class ScopedDeltaTime
{
public:
    explicit ScopedDeltaTime(double value) : previous(Time::deltaTime) { Time::deltaTime = value; }
    ~ScopedDeltaTime() { Time::deltaTime = previous; }
private:
    double previous;
};

class ScopedWorkingDirectory
{
public:
    explicit ScopedWorkingDirectory(const std::filesystem::path& path)
        : previous(std::filesystem::current_path())
    {
        std::filesystem::current_path(path);
    }
    ~ScopedWorkingDirectory()
    {
        std::error_code error;
        std::filesystem::current_path(previous, error);
    }
private:
    std::filesystem::path previous;
};

class SceneAssetsWithoutMusic
{
public:
    SceneAssetsWithoutMusic()
        : path(std::filesystem::temp_directory_path() /
               ("worms-scene-assets-" + std::to_string(SDL_GetPerformanceCounter())))
    {
        std::filesystem::create_directory(path);
        try
        {
            for (const auto& entry :
                 std::filesystem::directory_iterator(std::filesystem::current_path()))
            {
                const auto extension = entry.path().extension();
                if (entry.is_regular_file() && (extension == ".png" || extension == ".wav"))
                    std::filesystem::copy_file(entry.path(), path / entry.path().filename());
            }
        }
        catch (...)
        {
            Remove();
            throw;
        }
    }
    ~SceneAssetsWithoutMusic() { Remove(); }
    const std::filesystem::path& Directory() const { return path; }
    SceneAssetsWithoutMusic(const SceneAssetsWithoutMusic&) = delete;
    SceneAssetsWithoutMusic& operator=(const SceneAssetsWithoutMusic&) = delete;
private:
    void Remove() noexcept
    {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    std::filesystem::path path;
};

std::size_t CountOpaquePixels(SDL_Surface* surface)
{
    Sdl::SurfaceLock lock(surface);
    std::size_t count = 0;
    for (int y = 0; y < surface->h; ++y)
        for (int x = 0; x < surface->w; ++x)
        {
            const auto* row = reinterpret_cast<const Uint32*>(
                static_cast<const Uint8*>(surface->pixels) + y * surface->pitch);
            Uint8 r, g, b, a;
            SDL_GetRGBA(row[x], surface->format, &r, &g, &b, &a);
            if (a) ++count;
        }
    return count;
}

class ScopedDriverHint
{
public:
    ScopedDriverHint(const char* name, const char* value) : name(name)
    {
        if (const char* oldValue = SDL_GetHint(name))
            previous = oldValue;
        SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE);
    }
    ~ScopedDriverHint()
    {
        SDL_ResetHint(name);
        if (previous)
            SDL_SetHintWithPriority(name, previous->c_str(), SDL_HINT_OVERRIDE);
    }
private:
    const char* name;
    std::optional<std::string> previous;
};

class InspectableGame : public Game
{
public:
    World& Registry() { return GameTestAccess::Registry(*this); }
    b2World& Physics() { return GameTestAccess::Physics(*this); }
    SDL_Renderer* Renderer() { return renderer; }
    bool HasResources() const { return GameTestAccess::HasScene(*this) || renderer || window; }
};

class TrackedWorm : public Worm
{
public:
    TrackedWorm(SDL_Renderer* renderer, World* world, b2World* physics, const Camera& camera,
                SDL_Texture* texture, int& destroyed)
        : Worm(renderer, world, physics, camera, texture), destroyed(destroyed) {}
    ~TrackedWorm() override { ++destroyed; }
private:
    int& destroyed;
};

class GameLifetime : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(GameObject::activeObjs.empty());
        ASSERT_TRUE(GameObject::objsToAdd.empty());
        ASSERT_NO_THROW(game.InitWindow("Lifetime test", 800, 600));
        for (const auto& object : GameObject::activeObjs)
        {
            if (auto* candidate = dynamic_cast<Camera*>(object.get()))
                camera = candidate;
            if (auto* candidate = dynamic_cast<Weapon*>(object.get()))
                weapon = candidate;
        }
        ASSERT_NE(camera, nullptr);
        ASSERT_NE(weapon, nullptr);
        initialEntities = game.Registry().GetAmountOfAvailableEntities();
        initialBodies = game.Physics().GetBodyCount();
    }

    void TearDown() override
    {
        if (manager)
        {
            manager->CleanUp();
            manager.reset();
        }
        if (team)
        {
            team->CleanUp();
            team.reset();
        }
        game.Clean();
        EXPECT_FALSE(game.HasResources());
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
        EXPECT_FALSE(ContactManager::Get().TakePendingException());
        EXPECT_TRUE(GameObject::activeObjs.empty());
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
    }

    void CreateTestTeam()
    {
        WormTeam::TexturePtr texture(
            SDL_CreateTexture(game.Renderer(), SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STATIC, 40, 10));
        ASSERT_NE(texture.get(), nullptr);
        team = std::make_unique<WormTeam>(std::move(texture));
    }

    Worm* AddTrackedWorm()
    {
        auto worm = std::make_unique<TrackedWorm>(
            game.Renderer(), &game.Registry(), &game.Physics(), *camera,
            team->GetHealthBarTexture(), destroyedWorms);
        auto* pointer = worm.get();
        team->AddWorm(std::move(worm));
        return pointer;
    }

    void CreateManager()
    {
        manager = std::make_unique<WormManager>(
            game.Renderer(), &game.Registry(), &game.Physics(), *camera, *weapon);
    }

    void KillActiveWorm()
    {
        auto& body = game.Registry().GetComponent<RigidBody>(manager->GetActiveWormId());
        body.body->SetTransform(b2Vec2{0, -20}, 0);
        game.Registry().GetComponent<Position>(manager->GetActiveWormId()).y = -20;
        manager->Update();
    }

    ScopedDriverHint video{SDL_HINT_VIDEODRIVER, "dummy"};
    ScopedDriverHint audio{SDL_HINT_AUDIODRIVER, "dummy"};
    ScopedDriverHint render{SDL_HINT_RENDER_DRIVER, "software"};
    InspectableGame game;
    Camera* camera = nullptr;
    Weapon* weapon = nullptr;
    int destroyedWorms = 0;
    std::unique_ptr<WormTeam> team;
    std::unique_ptr<WormManager> manager;
    uint16_t initialEntities = 0;
    int initialBodies = 0;
};

TEST_F(GameLifetime, RejectedSecondSceneDoesNotCleanTheRunningScene)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    {
        GameScene second(game.Renderer());
        EXPECT_THROW(second.Initialize(), std::logic_error);
        EXPECT_NO_THROW(second.CleanUp());
    }
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    EXPECT_NO_THROW(game.Update());
}

TEST_F(GameLifetime, SceneDestructionReleasesGameplayButKeepsPlatformAlive)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    GameTestAccess::ResetScene(game);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
    EXPECT_TRUE(GameObject::activeObjs.empty());
    EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0);
    EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
    EXPECT_NE(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        SCOPED_TRACE(cycle);
        {
            GameScene scene(game.Renderer());
            scene.Initialize();
            EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
            EXPECT_THROW(scene.Initialize(), std::logic_error);
            EXPECT_NO_THROW(scene.Update());
            EXPECT_NO_THROW(scene.Render());
            EXPECT_NO_THROW(scene.RenderDebug());
            GameObject::objsToAdd.emplace_back(
                std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 2));
        }
        EXPECT_TRUE(GameObject::activeObjs.empty());
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
        EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);
    }
}

TEST_F(GameLifetime, FailedSceneInitializationRollsBackAndCanBeRetried)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    GameTestAccess::ResetScene(game);
    GameScene scene(game.Renderer());
    {
        // The project root has no gameplay assets, so initialization fails after physics setup.
        ScopedWorkingDirectory directory(std::filesystem::current_path().parent_path());
        EXPECT_THROW(scene.Initialize(), SDL_Exception);
    }
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_TRUE(GameObject::activeObjs.empty());
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
    EXPECT_NO_THROW(scene.CleanUp());
    EXPECT_NO_THROW(scene.Update());
    EXPECT_NO_THROW(scene.Render());
    EXPECT_NO_THROW(scene.RenderDebug());
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    scene.CleanUp();
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_NO_THROW(scene.CleanUp());
}

TEST_F(GameLifetime, FramePresentationAndColliderDebugStillWork)
{
    game.PreRender();
    EXPECT_NO_THROW(game.Render());
    EXPECT_NO_THROW(game.PostRender());
}

TEST_F(GameLifetime, CleanupExceptionsDoNotStopSceneDestructionOrNextSceneStartup)
{
    struct CleanupObject : GameObject
    {
        CleanupObject(int failure, int& cleaned, int& destroyed)
            : failure(failure), cleaned(cleaned), destroyed(destroyed) {}
        void CleanUp() override
        {
            ++cleaned;
            if (failure == 1) throw std::runtime_error("Cleanup failure");
            if (failure == 2) throw 42;
            GameObject::CleanUp();
        }
        ~CleanupObject() override { ++destroyed; }
        int failure;
        int& cleaned;
        int& destroyed;
    };

    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    int cleaned = 0;
    int destroyed = 0;
    for (int failure : {1, 0})
    {
        auto object = std::make_unique<CleanupObject>(failure, cleaned, destroyed);
        object->Initialise(game.Renderer(), &game.Registry());
        GameObject::activeObjs.emplace_back(std::move(object));
    }
    for (int failure : {2, 0})
        GameObject::objsToAdd.emplace_back(
            std::make_unique<CleanupObject>(failure, cleaned, destroyed));

    EXPECT_NO_THROW(GameTestAccess::ResetScene(game));
    EXPECT_EQ(cleaned, 4);
    EXPECT_EQ(destroyed, 4);
    EXPECT_TRUE(GameObject::activeObjs.empty());
    EXPECT_TRUE(GameObject::objsToAdd.empty());
    EXPECT_TRUE(GameObject::objsToDelete.empty());
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
    EXPECT_FALSE(ContactManager::Get().TakePendingException());
    EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);

    GameScene next(game.Renderer());
    ASSERT_NO_THROW(next.Initialize());
    EXPECT_EQ(GameTestAccess::Registry(next).GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(GameTestAccess::Physics(next).GetBodyCount(), initialBodies);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
}

TEST_F(GameLifetime, MissingMusicRollsBackFullyConstructedGameplayAndAllowsRetry)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    GameTestAccess::ResetScene(game);
    GameScene scene(game.Renderer());
    SceneAssetsWithoutMusic assets;
    {
        ScopedWorkingDirectory directory(assets.Directory());
        try
        {
            scene.Initialize();
            FAIL() << "Missing music should fail scene initialization";
        }
        catch (const SDL_Exception& error)
        {
            // Music is loaded only after teams, map, camera and weapon assets are ready.
            EXPECT_EQ(std::filesystem::path(error.GetFile()).filename(), "Music.cpp");
        }
    }
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_TRUE(GameObject::activeObjs.empty());
    EXPECT_TRUE(GameObject::objsToAdd.empty());
    EXPECT_TRUE(GameObject::objsToDelete.empty());
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
    EXPECT_FALSE(ContactManager::Get().TakePendingException());
    EXPECT_EQ(Mix_PlayingMusic(), 0);
    EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);
    EXPECT_NO_THROW(scene.CleanUp());

    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(GameTestAccess::Registry(scene).GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(GameTestAccess::Physics(scene).GetBodyCount(), initialBodies);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    EXPECT_NO_THROW(scene.CleanUp());
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
}

TEST_F(GameLifetime, RemovingActiveWormSelectsSuccessorAndReleasesItsResources)
{
    CreateTestTeam();
    ASSERT_NE(team, nullptr);
    auto* first = AddTrackedWorm();
    const auto removedId = first->GetId();
    const auto secondId = AddTrackedWorm()->GetId();
    team->RemoveWorm(first);
    EXPECT_EQ(team->Size(), 1);
    EXPECT_EQ(team->GetActiveWorm(), secondId);
    EXPECT_EQ(destroyedWorms, 1);
    EXPECT_FALSE(game.Registry().TryGetComponent<Position>(removedId));
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 1);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 3);
}

TEST_F(GameLifetime, RemovingEarlierInactiveWormPreservesTheActiveWorm)
{
    CreateTestTeam();
    ASSERT_NE(team, nullptr);
    auto* first = AddTrackedWorm();
    AddTrackedWorm();
    const auto thirdId = AddTrackedWorm()->GetId();
    team->ChangeActiveWorm();
    team->ChangeActiveWorm();
    team->RemoveWorm(first);
    EXPECT_EQ(team->GetActiveWorm(), thirdId);
    EXPECT_EQ(team->Size(), 2);
    EXPECT_EQ(destroyedWorms, 1);
}

TEST_F(GameLifetime, RemovingLastWormAndRepeatedCleanupRestoreResourceCounts)
{
    CreateTestTeam();
    ASSERT_NE(team, nullptr);
    auto* worm = AddTrackedWorm();
    team->RemoveWorm(worm);
    EXPECT_EQ(team->Size(), 0);
    EXPECT_EQ(destroyedWorms, 1);
    EXPECT_THROW(team->GetActiveWorm(), std::logic_error);
    EXPECT_NO_THROW(team->CleanUp());
    EXPECT_NO_THROW(team->CleanUp());
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, CleanupDestroysAllRemainingWorms)
{
    CreateTestTeam();
    ASSERT_NE(team, nullptr);
    AddTrackedWorm();
    AddTrackedWorm();
    team->CleanUp();
    EXPECT_EQ(destroyedWorms, 2);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_NO_THROW(team->CleanUp());
}

TEST_F(GameLifetime, EliminatedTeamIsReleasedAndNextTurnDoesNotSkipAnotherTeam)
{
    CreateManager();
    manager->CreateTeam(1);
    manager->CreateTeam(1);
    manager->CreateTeam(1);
    const auto eliminatedId = manager->GetActiveWormId();
    KillActiveWorm();
    const auto selectedId = manager->GetActiveWormId();
    EXPECT_NE(selectedId, eliminatedId);
    EXPECT_FALSE(game.Registry().TryGetComponent<Position>(eliminatedId));
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 2);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 6);
    ASSERT_TRUE(camera->noTargetEvent);
    camera->noTargetEvent();
    EXPECT_EQ(manager->GetActiveWormId(), selectedId);
}

TEST_F(GameLifetime, EliminatingLastTeamAndRepeatedCleanupLeaveNoTeamResources)
{
    CreateManager();
    manager->CreateTeam(1);
    KillActiveWorm();
    EXPECT_THROW(manager->GetActiveWormId(), std::logic_error);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_NO_THROW(manager->CleanUp());
    EXPECT_NO_THROW(manager->CleanUp());
    EXPECT_FALSE(camera->noTargetEvent);
}

TEST_F(GameLifetime, FailedTeamConstructionRollsBackBodiesAndAuxiliaryEntities)
{
    auto incompleteWorld = std::make_unique<World>(game.Renderer());
    incompleteWorld->RegisterComponent<Position>();
    incompleteWorld->RegisterComponent<Sprite>();
    incompleteWorld->RegisterComponent<Health>();
    incompleteWorld->RegisterComponent<RigidBody>();
    const auto available = incompleteWorld->GetAmountOfAvailableEntities();
    // Missing Follow fails inside HealthBar, after the worm's body has been created.
    WormManager failing(game.Renderer(), incompleteWorld.get(), &game.Physics(), *camera, *weapon);
    EXPECT_THROW(failing.CreateTeam(2), std::exception);
    EXPECT_EQ(incompleteWorld->GetAmountOfAvailableEntities(), available);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_THROW(failing.GetActiveWormId(), std::logic_error);
    failing.CleanUp();
}

TEST_F(GameLifetime, FailedTextureLoadCanBeCleanedWithoutLeakingAnEntity)
{
    auto effect = std::make_unique<ParticleSystem>("missing-lifetime-test-texture.png", 1, 0, 0, 10);
    auto* pointer = effect.get();
    GameObject::objsToAdd.emplace_back(std::move(effect));
    EXPECT_THROW(pointer->Initialise(game.Renderer(), &game.Registry()), SDL_Exception);
    EXPECT_FALSE(pointer->HasEntity());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, PartialParticleInitializationCanBeCleanedWithoutLeakingEntities)
{
    World incompleteWorld(game.Renderer());
    incompleteWorld.RegisterComponent<Particle>();
    incompleteWorld.RegisterComponent<Position>();
    const auto available = incompleteWorld.GetAmountOfAvailableEntities();
    ParticleSystem effect("blood.png", 1, 0, 0, 10);
    // Missing Motion fails after creating the first particle and its Position.
    EXPECT_THROW(effect.Initialise(game.Renderer(), &incompleteWorld), std::exception);
    EXPECT_FALSE(effect.HasEntity());
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), available);
    EXPECT_NO_THROW(effect.CleanUp());
    EXPECT_NO_THROW(effect.CleanUp());
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), available);
}

TEST_F(GameLifetime, GameClosesWithActiveProjectileParticlesAndPendingObjects)
{
    auto projectile = std::make_unique<Projectile>(0, 2, 1, 0);
    projectile->SetCamera(camera);
    projectile->SetGravityScale(1);
    projectile->SetMaxSpeed(2);
    auto* projectilePointer = projectile.get();
    GameObject::activeObjs.emplace_back(std::move(projectile));
    projectilePointer->Initialise(game.Renderer(), &game.Registry());

    auto particles = std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 10);
    auto* particlesPointer = particles.get();
    GameObject::activeObjs.emplace_back(std::move(particles));
    particlesPointer->Initialise(game.Renderer(), &game.Registry());
    GameObject::objsToAdd.emplace_back(std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 3));
    GameObject::objsToDelete.push_back(projectilePointer);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 1);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 12);

    EXPECT_NO_THROW(game.Clean());
    EXPECT_FALSE(game.HasResources());
    EXPECT_TRUE(GameObject::activeObjs.empty());
    EXPECT_TRUE(GameObject::objsToAdd.empty());
    EXPECT_TRUE(GameObject::objsToDelete.empty());
    EXPECT_EQ(SDL_WasInit(0), 0u);
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    EXPECT_EQ(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(game.Clean());
}

TEST_F(GameLifetime, ProjectileCanUnsubscribeDuringCollisionWithoutRemovingOtherListeners)
{
    auto projectile = std::make_unique<Projectile>(0, 2, 0, 0);
    auto* pointer = projectile.get();
    GameObject::activeObjs.emplace_back(std::move(projectile));
    pointer->Initialise(game.Renderer(), &game.Registry());
    const auto id = pointer->GetId();
    auto* body = game.Registry().GetComponent<RigidBody>(id).body;
    b2BodyDef definition;
    definition.position.Set(0, 2);
    auto* obstacle = game.Physics().CreateBody(&definition);
    b2PolygonShape shape;
    shape.SetAsBox(0.5f, 0.5f);
    b2FixtureDef fixture;
    fixture.shape = &shape;
    fixture.isSensor = true;
    obstacle->CreateFixture(&fixture);
    int calls = 0;
    auto observer = ContactManager::Get().AddEvent(id, BEGIN, [&](b2Contact*) { ++calls; });
    game.Physics().Step(1.f / 60, 8, 3);
    ASSERT_NE(body->GetContactList(), nullptr);
    const int previousCalls = calls;
    ContactManager::Get().BeginContact(body->GetContactList()->contact);
    EXPECT_EQ(calls, previousCalls + 1);
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_TRUE(ContactManager::Get().RemoveEvent(observer));
    game.Physics().DestroyBody(obstacle);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, CollisionExceptionIsReportedAfterStepAndDoesNotInterruptRepeatedGameCleanup)
{
    PhysicsInfo info{NONE, game.Registry().CreateEntity()};
    b2BodyDef definition;
    definition.position.Set(1000, 1000);
    auto* fixed = game.Physics().CreateBody(&definition);
    b2PolygonShape shape;
    shape.SetAsBox(0.5f, 0.5f);
    b2FixtureDef fixture;
    fixture.shape = &shape;
    fixture.isSensor = true;
    fixture.userData.pointer = reinterpret_cast<uintptr_t>(&info);
    fixed->CreateFixture(&fixture);
    definition.type = b2_dynamicBody;
    auto* moving = game.Physics().CreateBody(&definition);
    moving->CreateFixture(&shape, 1);
    int calls = 0;
    ContactManager::Get().AddEvent(info.id, BEGIN, [&](b2Contact*)
    {
        ++calls;
        throw std::runtime_error("Cleanup test collision failure");
    });
    const auto previousDelta = Time::deltaTime;
    Time::deltaTime = 1.0 / 60;
    EXPECT_THROW(game.Update(), std::runtime_error);
    Time::deltaTime = previousDelta;
    EXPECT_FALSE(game.Physics().IsLocked());
    EXPECT_EQ(calls, 1);
    EXPECT_FALSE(ContactManager::Get().TakePendingException());
    ASSERT_NE(moving->GetContactList(), nullptr);
    EXPECT_NO_THROW(ContactManager::Get().BeginContact(moving->GetContactList()->contact));
    EXPECT_EQ(calls, 2);
    EXPECT_NO_THROW(game.Clean());
    EXPECT_FALSE(game.HasResources());
    EXPECT_FALSE(ContactManager::Get().TakePendingException());
    EXPECT_NO_THROW(game.Clean());
}

TEST_F(GameLifetime, EmptyTerrainRebuildRemovesBodyAndCleanupCanBeRepeated)
{
    Map* map = nullptr;
    for (const auto& object : GameObject::activeObjs)
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    ASSERT_NE(game.Registry().GetComponent<RigidBody>(map->GetId()).body, nullptr);
    ASSERT_EQ(SDL_FillRect(MapTestAccess::Surface(*map), nullptr, 0), 0);
    MapTestAccess::RequestRebuild(*map);
    EXPECT_NO_THROW(map->Update());
    EXPECT_EQ(game.Registry().GetComponent<RigidBody>(map->GetId()).body, nullptr);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies - 1);
    EXPECT_NO_THROW(map->CleanUp());
    EXPECT_NO_THROW(map->CleanUp());
    EXPECT_NO_THROW(map->Update());
}

TEST_F(GameLifetime, TinyTerrainContourDoesNotCreateInvalidBox2DLoop)
{
    Map* map = nullptr;
    for (const auto& object : GameObject::activeObjs)
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    auto* surface = MapTestAccess::Surface(*map);
    ASSERT_EQ(SDL_FillRect(surface, nullptr, 0), 0);
    SDL_Rect pixel{surface->w / 2, surface->h / 2, 1, 1};
    ASSERT_EQ(SDL_FillRect(surface, &pixel, SDL_MapRGBA(surface->format, 255, 255, 255, 255)), 0);
    MapTestAccess::RequestRebuild(*map);
    EXPECT_NO_THROW(map->Update());
    EXPECT_EQ(game.Registry().GetComponent<RigidBody>(map->GetId()).body, nullptr);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies - 1);
}

TEST_F(GameLifetime, FailedMapTextureReplacementPreservesOldResourcesAndCanBeRetried)
{
    Map* map = nullptr;
    for (const auto& object : GameObject::activeObjs)
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    const auto id = map->GetId();
    auto* oldTexture = game.Registry().GetComponent<Sprite>(id).texture;
    auto* oldBody = game.Registry().GetComponent<RigidBody>(id).body;
    MapTestAccess::RequestRebuild(*map);
    EXPECT_THROW(MapTestAccess::UpdateWithoutRenderer(*map), SDL_Exception);
    EXPECT_EQ(game.Registry().GetComponent<Sprite>(id).texture, oldTexture);
    EXPECT_EQ(game.Registry().GetComponent<RigidBody>(id).body, oldBody);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_TRUE(MapTestAccess::HasPendingRebuild(*map));
    EXPECT_EQ(SDL_QueryTexture(oldTexture, nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(map->Update());
    EXPECT_FALSE(MapTestAccess::HasPendingRebuild(*map));
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, FailedMapColliderReplacementPreservesOldResourcesAndCanBeRetried)
{
    Map* map = nullptr;
    for (const auto& object : GameObject::activeObjs)
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    const auto id = map->GetId();
    auto* oldTexture = game.Registry().GetComponent<Sprite>(id).texture;
    auto* oldBody = game.Registry().GetComponent<RigidBody>(id).body;
    MapTestAccess::RequestRebuild(*map);
    ColliderFactory::Get().Init(nullptr);
    EXPECT_THROW(map->Update(), std::logic_error);
    ColliderFactory::Get().Init(&game.Physics());
    EXPECT_EQ(game.Registry().GetComponent<Sprite>(id).texture, oldTexture);
    EXPECT_EQ(game.Registry().GetComponent<RigidBody>(id).body, oldBody);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_TRUE(MapTestAccess::HasPendingRebuild(*map));
    EXPECT_NO_THROW(map->Update());
    EXPECT_FALSE(MapTestAccess::HasPendingRebuild(*map));
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, FailedMapInitializationAutomaticallyReleasesResourcesAndSubscription)
{
    World incompleteWorld(game.Renderer());
    incompleteWorld.RegisterComponent<Position>();
    incompleteWorld.RegisterComponent<RigidBody>();
    const auto entities = incompleteWorld.GetAmountOfAvailableEntities();
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    Map map(&game.Physics());
    // Missing Sprite fails after loading the surface, texture and callback.
    EXPECT_THROW(map.Initialise(game.Renderer(), &incompleteWorld), std::exception);
    EXPECT_FALSE(map.HasEntity());
    EXPECT_FALSE(MapTestAccess::HasResources(map));
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), entities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    EXPECT_NO_THROW(map.CleanUp());
    EXPECT_NO_THROW(map.Update());
    EXPECT_NO_THROW(map.DestroyMapAtLocalPoint({0, 0}));
}

TEST_F(GameLifetime, FailedProjectileInitializationAutomaticallyReleasesBodyAndSubscription)
{
    Camera uninitializedCamera;
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    Projectile projectile(0, 2, 0, 0);
    projectile.SetCamera(&uninitializedCamera);
    // ChangeTarget fails after creating the projectile's body and subscription.
    EXPECT_THROW(projectile.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
    EXPECT_FALSE(projectile.HasEntity());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    EXPECT_NO_THROW(projectile.CleanUp());
    EXPECT_NO_THROW(projectile.Update());
}

TEST_F(GameLifetime, FailedCameraInitializationAutomaticallyReleasesItsEntity)
{
    World incompleteWorld(game.Renderer());
    const auto available = incompleteWorld.GetAmountOfAvailableEntities();
    Camera localCamera;
    EXPECT_THROW(localCamera.Initialise(game.Renderer(), &incompleteWorld), std::exception);
    EXPECT_FALSE(localCamera.HasEntity());
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), available);
    EXPECT_NO_THROW(localCamera.CleanUp());
    EXPECT_NO_THROW(localCamera.Update());
}

TEST_F(GameLifetime, FailedWeaponTextureLoadAutomaticallyReleasesItsEntity)
{
    Weapon localWeapon(*camera);
    EXPECT_THROW(localWeapon.Initialise(nullptr, &game.Registry()), SDL_Exception);
    EXPECT_FALSE(localWeapon.HasEntity());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_NO_THROW(localWeapon.CleanUp());
    EXPECT_NO_THROW(localWeapon.Update());
    EXPECT_NO_THROW(localWeapon.Render());
}

TEST_F(GameLifetime, CleanedHealthBarDoesNotReleaseBorrowedTexture)
{
    Sdl::TexturePtr texture(SDL_CreateTexture(game.Renderer(), SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STATIC, 40, 10));
    ASSERT_TRUE(texture);
    HealthBar bar(game.Renderer(), &game.Registry(), camera->GetId(), *camera, 100, texture.get());
    bar.CleanUp();
    EXPECT_FALSE(bar.HasEntity());
    EXPECT_EQ(SDL_QueryTexture(texture.get(), nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(bar.CleanUp());
    EXPECT_NO_THROW(bar.Render());
    EXPECT_NO_THROW(bar.TakeDamage(10));
    EXPECT_THROW(bar.getCurrentHp(), std::logic_error);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, RemovingWormPreservesSharedHealthTextureAndCleanedWormCanBeRendered)
{
    CreateTestTeam();
    ASSERT_TRUE(team);
    auto* first = AddTrackedWorm();
    auto* second = AddTrackedWorm();
    auto* texture = team->GetHealthBarTexture();
    first->CleanUp();
    EXPECT_NO_THROW(first->Render());
    EXPECT_NO_THROW(first->Jump());
    team->RemoveWorm(first);
    EXPECT_EQ(SDL_QueryTexture(texture, nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(second->Render());
}

TEST_F(GameLifetime, WeaponAndProjectileCleanupPreserveBorrowedTexture)
{
    Sdl::TexturePtr texture(SDL_CreateTexture(game.Renderer(), SDL_PIXELFORMAT_RGBA8888,
                                            SDL_TEXTUREACCESS_STATIC, 10, 10));
    ASSERT_TRUE(texture);
    Weapon localWeapon(*camera);
    localWeapon.Initialise(game.Renderer(), &game.Registry());
    localWeapon.SetTexture(texture.get());
    localWeapon.SetProjectileTexture(texture.get());
    Projectile projectile(0, 2, 0, 0);
    projectile.SetTexture(texture.get());
    projectile.Initialise(game.Renderer(), &game.Registry());
    localWeapon.CleanUp();
    projectile.CleanUp();
    EXPECT_EQ(SDL_QueryTexture(texture.get(), nullptr, nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(localWeapon.Render());
    EXPECT_NO_THROW(localWeapon.Update());
    EXPECT_NO_THROW(projectile.Update());
    EXPECT_NO_THROW(localWeapon.CleanUp());
    EXPECT_NO_THROW(projectile.CleanUp());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, RepeatedObjectLifecyclesRestoreEntitiesBodiesAndSubscriptions)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    for (int i = 0; i < 5; ++i)
    {
        ParticleSystem particles("blood.png", 1, 0, 0, 4);
        particles.Initialise(game.Renderer(), &game.Registry());
        EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(particles), 1);
        const auto particleId = particles.GetId();
        EXPECT_THROW(particles.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(particles.GetId(), particleId);
        particles.CleanUp();
        EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(particles), 0);
        EXPECT_EQ(ParticleSystemTestAccess::ParticleCount(particles), 0);
        EXPECT_NO_THROW(particles.CleanUp());
        EXPECT_NO_THROW(particles.Update());

        Camera localCamera;
        localCamera.Initialise(game.Renderer(), &game.Registry());
        const auto cameraId = localCamera.GetId();
        EXPECT_THROW(localCamera.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(localCamera.GetId(), cameraId);
        localCamera.CleanUp();
        EXPECT_NO_THROW(localCamera.CleanUp());
        EXPECT_NO_THROW(localCamera.Update());

        Weapon localWeapon(*camera);
        localWeapon.Initialise(game.Renderer(), &game.Registry());
        EXPECT_EQ(WeaponTestAccess::OwnedResources(localWeapon), 1);
        const auto weaponId = localWeapon.GetId();
        EXPECT_THROW(localWeapon.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(localWeapon.GetId(), weaponId);
        localWeapon.CleanUp();
        EXPECT_EQ(WeaponTestAccess::OwnedResources(localWeapon), 0);
        EXPECT_NO_THROW(localWeapon.CleanUp());

        Projectile projectile(0, 2, 0, 0);
        projectile.Initialise(game.Renderer(), &game.Registry());
        const auto projectileId = projectile.GetId();
        EXPECT_THROW(projectile.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(projectile.GetId(), projectileId);
        projectile.CleanUp();
        EXPECT_NO_THROW(projectile.CleanUp());

        Map map(&game.Physics());
        map.Initialise(game.Renderer(), &game.Registry());
        EXPECT_EQ(MapTestAccess::OwnedResources(map), 2);
        const auto mapId = map.GetId();
        EXPECT_THROW(map.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(map.GetId(), mapId);
        map.CleanUp();
        EXPECT_EQ(MapTestAccess::OwnedResources(map), 0);
        EXPECT_NO_THROW(map.CleanUp());

        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
}

TEST_F(GameLifetime, ExpiredParticlesAreRemovedThroughTheGameQueueAndRestoreCounts)
{
    class ExpiringParticles : public ParticleSystem
    {
    public:
        explicit ExpiringParticles(std::shared_ptr<int> destroyed)
            : ParticleSystem("blood.png", 1, 100, 100, 4), destroyed(destroyed) {}
        ~ExpiringParticles() override
        {
            EXPECT_FALSE(HasEntity());
            EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(*this), 0);
            EXPECT_EQ(ParticleSystemTestAccess::ParticleCount(*this), 0);
            ++*destroyed;
        }
    private:
        std::shared_ptr<int> destroyed;
    };
    ScopedDeltaTime delta(0);
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    const auto active = GameObject::activeObjs.size();
    auto destroyed = std::make_shared<int>(0);
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        SCOPED_TRACE(cycle);
        auto effect = std::make_unique<ExpiringParticles>(destroyed);
        auto* pointer = effect.get();
        GameObject::objsToAdd.emplace_back(std::move(effect));
        game.Update();
        const auto handle = game.Registry().GetHandle(pointer->GetId());
        ASSERT_TRUE(handle);
        EXPECT_EQ(ParticleSystemTestAccess::ParticleCount(*pointer), 4);
        EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(*pointer), 1);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 5);
        {
            ScopedDeltaTime expire(6);
            pointer->Update();
        }
        EXPECT_EQ(GameObject::objsToDelete.size(), 1);
        game.Update();
        EXPECT_FALSE(game.Registry().IsAlive(*handle));
        EXPECT_EQ(*destroyed, cycle + 1);
        EXPECT_EQ(GameObject::activeObjs.size(), active);
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
}

TEST_F(GameLifetime, ExplosionAndParticleExpiryRestoreCountsAcrossRepeatedGameUpdates)
{
    ScopedDeltaTime delta(0);
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    const auto active = GameObject::activeObjs.size();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        auto projectile = std::make_unique<Projectile>(100, -20, 0, 0);
        projectile->SetExplosionRadius(0.5f);
        projectile->Initialise(game.Renderer(), &game.Registry());
        const auto handle = game.Registry().GetHandle(projectile->GetId());
        ASSERT_TRUE(handle);
        GameObject::activeObjs.emplace_back(std::move(projectile));
        game.Update();
        EXPECT_EQ(GameObject::objsToAdd.size(), 1);
        EXPECT_EQ(GameObject::objsToDelete.size(), 1);
        game.Update();
        EXPECT_FALSE(game.Registry().IsAlive(*handle));
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 101);
        for (const auto& object : GameObject::activeObjs)
            if (auto* particles = dynamic_cast<ParticleSystem*>(object.get()))
            {
                ScopedDeltaTime expire(6);
                particles->Update();
            }
        game.Update();
        EXPECT_EQ(GameObject::activeObjs.size(), active);
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
}

TEST_F(GameLifetime, RepeatedLastTeamDeathsAndTheirParticlesRestoreCounts)
{
    ScopedDeltaTime delta(0);
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    const auto active = GameObject::activeObjs.size();
    CreateManager();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        manager->CreateTeam(1);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 3);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 1);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions + 3);
        KillActiveWorm();
        EXPECT_THROW(manager->GetActiveWormId(), std::logic_error);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
        EXPECT_EQ(GameObject::objsToAdd.size(), 1);
        game.Update();
        for (const auto& object : GameObject::activeObjs)
            if (auto* particles = dynamic_cast<ParticleSystem*>(object.get()))
            {
                ScopedDeltaTime expire(6);
                particles->Update();
            }
        game.Update();
        EXPECT_EQ(GameObject::activeObjs.size(), active);
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
}

TEST_F(GameLifetime, DuplicateDeletionRequestsCleanAndDestroyAnObjectOnlyOnce)
{
    class DeletionProbe : public GameObject
    {
    public:
        DeletionProbe(int& cleaned, int& destroyed) : cleaned(cleaned), destroyed(destroyed) {}
        ~DeletionProbe() override { ++destroyed; }
        void CleanUp() override { ++cleaned; GameObject::CleanUp(); }
    private:
        int& cleaned;
        int& destroyed;
    };
    ScopedDeltaTime delta(0);
    int cleaned = 0, destroyed = 0;
    auto object = std::make_unique<DeletionProbe>(cleaned, destroyed);
    object->Initialise(game.Renderer(), &game.Registry());
    const auto handle = game.Registry().GetHandle(object->GetId());
    GameObject::objsToDelete.push_back(object.get());
    GameObject::objsToDelete.push_back(object.get());
    GameObject::activeObjs.emplace_back(std::move(object));
    game.Update();
    EXPECT_EQ(cleaned, 1);
    EXPECT_EQ(destroyed, 1);
    EXPECT_FALSE(game.Registry().IsAlive(*handle));
    EXPECT_TRUE(GameObject::objsToDelete.empty());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, RepeatedTerrainDeformationReplacesResourcesWithoutGrowingCounts)
{
    Map* map = nullptr;
    for (const auto& object : GameObject::activeObjs)
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    auto* surface = MapTestAccess::Surface(*map);
    ASSERT_EQ(surface->format->BytesPerPixel, 4);
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        SDL_Point pixel{-1, -1};
        {
            Sdl::SurfaceLock lock(surface);
            for (int y = 0; y < surface->h && pixel.x < 0; ++y)
                for (int x = 0; x < surface->w; ++x)
                {
                    const auto* row = reinterpret_cast<const Uint32*>(
                        static_cast<const Uint8*>(surface->pixels) + y * surface->pitch);
                    Uint8 r, g, b, a;
                    SDL_GetRGBA(row[x], surface->format, &r, &g, &b, &a);
                    if (a) { pixel = {x, y}; break; }
                }
        }
        ASSERT_GE(pixel.x, 0);
        const auto before = CountOpaquePixels(surface);
        const auto pos = game.Registry().GetComponent<Position>(map->GetId());
        const Position impact{pos.x + (pixel.x - surface->w / 2) / 100.f,
                              pos.y - (pixel.y - surface->h / 2) / 100.f};
        MapTestAccess::RequestDeformation(*map, impact, 0.2f);
        map->Update();
        EXPECT_LT(CountOpaquePixels(surface), before);
        EXPECT_EQ(MapTestAccess::OwnedResources(*map), 2);
        EXPECT_EQ(SDL_QueryTexture(game.Registry().GetComponent<Sprite>(map->GetId()).texture,
                                  nullptr, nullptr, nullptr, nullptr), 0);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
}

TEST_F(GameLifetime, RepeatedFullGameStartupAndShutdownReleaseSubsystemsAndSubscriptions)
{
    const auto subscriptions = ContactManager::Get().GetSubscriptionCount();
    game.Clean();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        game.InitWindow("Repeated lifetime test", 800, 600);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
        auto particles = std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 4);
        particles->Initialise(game.Renderer(), &game.Registry());
        GameObject::activeObjs.emplace_back(std::move(particles));
        auto projectile = std::make_unique<Projectile>(100, 100, 0, 0);
        projectile->Initialise(game.Renderer(), &game.Registry());
        GameObject::activeObjs.emplace_back(std::move(projectile));
        GameObject::objsToAdd.emplace_back(std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 2));
        game.Clean();
        EXPECT_FALSE(game.HasResources());
        EXPECT_TRUE(GameObject::activeObjs.empty());
        EXPECT_TRUE(GameObject::objsToAdd.empty());
        EXPECT_TRUE(GameObject::objsToDelete.empty());
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), 0);
        EXPECT_FALSE(ContactManager::Get().TakePendingException());
        EXPECT_EQ(SDL_WasInit(0), 0);
        EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
        EXPECT_EQ(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
        EXPECT_NO_THROW(game.Clean());
    }
}

TEST_F(GameLifetime, MapReacquiresComponentsAfterBodyDestructionCallbacksCompactThem)
{
    auto& world = game.Registry();
    const auto earlier = world.CreateEntity();
    world.AddComponent<Sprite>(earlier);
    world.AddComponent<RigidBody>(earlier);
    Map map(&game.Physics());
    map.Initialise(game.Renderer(), &world);

    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(1.5f, -2.f);
    auto* sensor = game.Physics().CreateBody(&bodyDef);
    b2CircleShape shape;
    shape.m_radius = 100.f;
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &shape;
    fixtureDef.isSensor = true;
    sensor->CreateFixture(&fixtureDef);
    int callbacks = 0;
    const auto subscription = ContactManager::Get().AddEvent(map.GetId(), CollisionType::END,
        [&](b2Contact*)
        {
            ++callbacks;
            world.DestroyEntity(earlier);
        });
    game.Physics().Step(1.f / 60, 8, 3);
    MapTestAccess::RequestRebuild(map);
    EXPECT_NO_THROW(map.Update());
    EXPECT_GT(callbacks, 0);
    EXPECT_FALSE(world.IsAlive(earlier));
    EXPECT_NE(world.GetComponent<Sprite>(map.GetId()).texture, nullptr);
    EXPECT_NE(world.GetComponent<RigidBody>(map.GetId()).body, nullptr);
    EXPECT_NO_THROW(ContactManager::Get().RethrowPendingException());
    ContactManager::Get().RemoveEvent(subscription);
    map.CleanUp();
    game.Physics().DestroyBody(sensor);
}

TEST_F(GameLifetime, ObserversRejectAReusedTargetBeforeTheirNextUpdate)
{
    auto& world = game.Registry();
    const auto target = world.CreateEntity();
    world.AddComponent<Position>(target, {1, 2});
    const auto follower = world.CreateEntity();
    world.AddComponent<Position>(follower, {5, 6});
    world.AddComponent<Follow>(follower, {world.GetHandle(target), 0, 0});
    FocusPoint focus(game.Renderer(), &world);
    focus.ChangeTarget(target);
    camera->ChangeTarget(target);
    weapon->SetParent(target);
    WeaponTestAccess::SetCharge(*weapon);
    const auto pending = GameObject::objsToAdd.size();
    while (world.GetAmountOfAvailableEntities()) world.CreateEntity();
    world.DestroyEntity(target);
    const auto replacement = world.CreateEntity();
    ASSERT_EQ(replacement, target);
    world.AddComponent<Position>(replacement, {20, 30});

    EXPECT_FALSE(focus.GetPos());
    const auto cameraX = camera->X();
    const auto cameraY = camera->Y();
    camera->Update();
    EXPECT_FLOAT_EQ(camera->X(), cameraX);
    EXPECT_FLOAT_EQ(camera->Y(), cameraY);
    weapon->Update();
    EXPECT_FALSE(WeaponTestAccess::HasParent(*weapon));
    EXPECT_FLOAT_EQ(WeaponTestAccess::Charge(*weapon), 0);
    EXPECT_EQ(GameObject::objsToAdd.size(), pending);
    world.Update();
    EXPECT_FALSE(world.GetComponent<Follow>(follower).id);
    EXPECT_FLOAT_EQ(world.GetComponent<Position>(follower).x, 5);
    focus.CleanUp();
}

TEST_F(GameLifetime, SettingTheSameWeaponParentPreservesChargeButChangingItResetsCharge)
{
    auto& world = game.Registry();
    const auto first = world.CreateEntity();
    const auto second = world.CreateEntity();
    world.AddComponent<Position>(first, {1, 2});
    world.AddComponent<Position>(second, {3, 4});
    weapon->SetParent(first);
    WeaponTestAccess::SetCharge(*weapon);
    weapon->SetParent(first);
    EXPECT_FLOAT_EQ(WeaponTestAccess::Charge(*weapon), 0.5f);
    weapon->SetParent(second);
    EXPECT_FLOAT_EQ(WeaponTestAccess::Charge(*weapon), 0);
    EXPECT_TRUE(WeaponTestAccess::HasParent(*weapon));
}

TEST_F(GameLifetime, WeaponRenderClearsMissingParentPositionEvenWhileInactive)
{
    auto& world = game.Registry();
    const auto target = world.CreateEntity();
    world.AddComponent<Position>(target, {1, 2});
    weapon->SetParent(target);
    weapon->Deactivate();
    WeaponTestAccess::SetCharge(*weapon);
    world.RemoveComponent<Position>(target);
    // A render attempt would fail if it tried to read the weapon's components.
    world.RemoveComponent<Position>(weapon->GetId());
    EXPECT_NO_THROW(weapon->Render());
    EXPECT_FALSE(WeaponTestAccess::HasParent(*weapon));
    EXPECT_FLOAT_EQ(WeaponTestAccess::Charge(*weapon), 0);
    EXPECT_NO_THROW(weapon->Update());
}

TEST_F(GameLifetime, SurfaceOwnershipMovesAndLocksAreReleasedAfterException)
{
    auto loaded = IMG_LoadPhysicTexture(game.Renderer(), "map.png");
    ASSERT_TRUE(loaded);
    auto* surface = loaded->surface.get();
    ASSERT_NE(surface, nullptr);
    auto moved = std::move(*loaded);
    EXPECT_FALSE(loaded->surface);
    EXPECT_EQ(moved.surface.get(), surface);
    EXPECT_EQ(surface->locked, 0);
    EXPECT_THROW(( [&]()
    {
        Sdl::SurfaceLock lock(surface);
        EXPECT_GT(surface->locked, 0);
        throw std::runtime_error("Surface lock test");
    }() ), std::runtime_error);
    EXPECT_EQ(surface->locked, 0);
}
}
