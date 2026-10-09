#include "Core/Audio/Music.h"
#include "Core/Audio/Sound.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Game.h"
#include "Game/Player/WormTeam.h"
#include "Game/Weapon/Projectile.h"
#include <SDL_mixer.h>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<Sound>);
static_assert(!std::is_copy_assignable_v<Sound>);
static_assert(!std::is_copy_constructible_v<Music>);
static_assert(!std::is_copy_assignable_v<Music>);

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
    static void UpdateWithoutRenderer(Map& map)
    {
        auto* renderer = map.renderer;
        map.renderer = nullptr;
        try { map.Update(); }
        catch (...) { map.renderer = renderer; throw; }
        map.renderer = renderer;
    }
};

namespace
{
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
    World& Registry() { return *world; }
    b2World& Physics() { return *physicsWorld; }
    SDL_Renderer* Renderer() { return renderer; }
    bool HasResources() const { return world || physicsWorld || renderer || window; }
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
        const auto particleId = particles.GetId();
        EXPECT_THROW(particles.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(particles.GetId(), particleId);
        particles.CleanUp();
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
        const auto weaponId = localWeapon.GetId();
        EXPECT_THROW(localWeapon.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(localWeapon.GetId(), weaponId);
        localWeapon.CleanUp();
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
        const auto mapId = map.GetId();
        EXPECT_THROW(map.Initialise(game.Renderer(), &game.Registry()), std::logic_error);
        EXPECT_EQ(map.GetId(), mapId);
        map.CleanUp();
        EXPECT_NO_THROW(map.CleanUp());

        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(ContactManager::Get().GetSubscriptionCount(), subscriptions);
    }
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
