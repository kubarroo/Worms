#include "Core/Audio/Music.h"
#include "Core/Audio/Sound.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Game.h"
#include "Game/GameScene.h"
#include "Game/Map/Map.h"
#include "Game/Player/WormManager.h"
#include "Game/Player/WormTeam.h"
#include "Game/Weapon/Projectile.h"
#include "Game/Weapon/WeaponManager.h"
#include <SDL_mixer.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_contact_manager.h>
#include <filesystem>
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

static_assert(!std::is_copy_constructible_v<Sound>);
static_assert(!std::is_copy_assignable_v<Sound>);
static_assert(!std::is_copy_constructible_v<Music>);
static_assert(!std::is_copy_assignable_v<Music>);
static_assert(!std::is_copy_constructible_v<GameScene>);
static_assert(!std::is_move_constructible_v<GameScene>);
static_assert(!std::is_copy_constructible_v<App>);
static_assert(!std::is_move_constructible_v<App>);
static_assert(noexcept(std::declval<GameScene&>().CleanUp()));
static_assert(noexcept(std::declval<App&>().Clean()));
static_assert(noexcept(std::declval<Game&>().Clean()));

struct GameTestAccess
{
    static World& Registry(Game& game) { return *game.scene->world; }
    static b2World& Physics(Game& game) { return *game.scene->physicsWorld; }
    static bool HasScene(const Game& game) { return static_cast<bool>(game.scene); }
    static void ResetScene(Game& game) { game.scene.reset(); }
    static GameScene& Scene(Game& game) { return *game.scene; }
    static std::size_t SubscriptionCount(const Game& game)
    {
        return game.scene ? SubscriptionCount(*game.scene) : 0;
    }
    static std::size_t SubscriptionCount(const GameScene& scene)
    {
        return scene.contacts ? scene.contacts->GetSubscriptionCount() : 0;
    }
    static std::exception_ptr TakePendingException(Game& game)
    {
        return game.scene ? TakePendingException(*game.scene) : nullptr;
    }
    static std::exception_ptr TakePendingException(GameScene& scene)
    {
        return scene.contacts ? scene.contacts->TakePendingException() : nullptr;
    }
    static EntityId ActiveWorm(Game& game)
    {
        return game.scene->wormManager->GetActiveWormId();
    }
    static void ChangeTurn(Game& game) { game.scene->wormManager->OnCameraTargetLost(); }
    static std::span<const std::unique_ptr<GameObject>> Objects(const Game& game)
    {
        if (!game.scene) return {};
        return game.scene->activeObjects;
    }
    static std::size_t PendingAddCount(const Game& game)
    {
        return game.scene ? game.scene->pendingAdds.size() : 0;
    }
    static std::size_t PendingRemovalCount(const Game& game)
    {
        return game.scene ? game.scene->pendingRemovals.size() : 0;
    }
    static World& Registry(GameScene& scene) { return *scene.world; }
    static b2World& Physics(GameScene& scene) { return *scene.physicsWorld; }
    static void ActivateStartupObject(GameScene& scene, std::unique_ptr<GameObject> object)
    {
        const auto initialized = std::exchange(scene.initialized, false);
        try
        {
            scene.AddObject(std::move(object));
        }
        catch (...)
        {
            scene.initialized = initialized;
            throw;
        }
        scene.initialized = initialized;
    }
    static bool HasResources(const GameScene& scene)
    {
        return scene.world || scene.physicsWorld || scene.b2DebugDraw || scene.wormManager ||
               scene.weaponManager || scene.music || scene.context || scene.ownsRuntime ||
               scene.failedStartupObject || scene.contacts || scene.colliders || scene.camera ||
               !scene.activeObjects.empty() || !scene.pendingAdds.empty() ||
               !scene.pendingRemovals.empty();
    }
};

struct MapTestAccess
{
    static SDL_Surface* Surface(Map& map) { return map.physTex->surface.get(); }
    static bool HasResources(const Map& map)
    {
        return map.mapTexture || map.physTex || map.mapBody;
    }
    static void RebuildWithoutPosition(Map& map)
    {
        const auto position = map.world->GetComponent<Position>(map.objectId);
        map.world->RemoveComponent<Position>(map.objectId);
        try
        {
            map.CreateNewColliders();
        }
        catch (...)
        {
            map.world->AddComponent<Position>(map.objectId, Position{position.x, position.y});
            throw;
        }
        map.world->AddComponent<Position>(map.objectId, Position{position.x, position.y});
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

class SceneAssetsWithout
{
public:
    explicit SceneAssetsWithout(const std::filesystem::path& missingAsset)
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
                if (entry.is_regular_file() && entry.path().filename() != missingAsset &&
                    (extension == ".png" || extension == ".wav" || extension == ".ogg"))
                    std::filesystem::copy_file(entry.path(), path / entry.path().filename());
            }
        }
        catch (...)
        {
            Remove();
            throw;
        }
    }
    ~SceneAssetsWithout() { Remove(); }
    const std::filesystem::path& Directory() const { return path; }
    SceneAssetsWithout(const SceneAssetsWithout&) = delete;
    SceneAssetsWithout& operator=(const SceneAssetsWithout&) = delete;
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
    GameScene& Scene() { return GameTestAccess::Scene(*this); }
    const SceneContext& Context()
    {
        return Scene().Context();
    }
    SceneContext ContextWith(World& world)
    {
        return {Renderer(), world, Physics(), Scene(), Context().colliders, Context().contacts};
    }
    SceneContext ContextWith(SDL_Renderer* renderer)
    {
        return {renderer, Registry(), Physics(), Scene(), Context().colliders, Context().contacts};
    }
    auto Objects() const { return GameTestAccess::Objects(*this); }
    auto PendingAddCount() const { return GameTestAccess::PendingAddCount(*this); }
    auto PendingRemovalCount() const { return GameTestAccess::PendingRemovalCount(*this); }
    GameObject& AddObject(std::unique_ptr<GameObject> object)
    {
        return Scene().AddObject(std::move(object));
    }
    void QueueAdd(std::unique_ptr<GameObject> object) { Scene().QueueAdd(std::move(object)); }
    void RequestDestroy(GameObject& object) { Scene().RequestDestroy(object); }
    bool HasResources() const { return GameTestAccess::HasScene(*this) || renderer || window; }
};

class TrackedWorm : public Worm
{
public:
    TrackedWorm(const Camera& camera, SDL_Texture* texture, int& destroyed)
        : Worm(camera, texture, {-1.f, 2.f}), destroyed(destroyed)
    {
    }
    ~TrackedWorm() override { ++destroyed; }
private:
    int& destroyed;
};

struct QueueProbeStats
{
    int initialized = 0;
    int updated = 0;
    int cleaned = 0;
    int destroyed = 0;
};

class QueueProbe : public GameObject
{
public:
    explicit QueueProbe(std::shared_ptr<QueueProbeStats> stats) : stats(std::move(stats)) {}
    ~QueueProbe() override { ++stats->destroyed; }
    void Initialise(const SceneContext& context) override
    {
        GameObject::Initialise(context);
        ++stats->initialized;
        if (onInitialize) onInitialize(*this);
    }
    void Update() override
    {
        ++stats->updated;
        if (onUpdate) onUpdate(*this);
    }
    void CleanUp() override
    {
        ++stats->cleaned;
        if (onCleanup) onCleanup(*this);
        GameObject::CleanUp();
    }
    ObjectCommands& Owner()
    {
        return Context().objects;
    }
    std::function<void(QueueProbe&)> onInitialize;
    std::function<void(QueueProbe&)> onUpdate;
    std::function<void(QueueProbe&)> onCleanup;
private:
    std::shared_ptr<QueueProbeStats> stats;
};

class GameLifetime : public testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_TRUE(game.Objects().empty());
        ASSERT_TRUE(game.PendingAddCount() == 0);
        ASSERT_NO_THROW(game.InitWindow("Lifetime test", 800, 600));
        for (const auto& object : game.Objects())
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
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
        EXPECT_FALSE(GameTestAccess::TakePendingException(game));
        EXPECT_TRUE(game.Objects().empty());
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
    }

    void CreateTestTeam()
    {
        WormTeam::TexturePtr texture(
            SDL_CreateTexture(game.Renderer(), SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_STATIC, 40, 10));
        ASSERT_NE(texture.get(), nullptr);
        team = std::make_unique<WormTeam>(std::move(texture));
        team->Initialise();
    }

    Worm* AddTrackedWorm()
    {
        auto worm =
            std::make_unique<TrackedWorm>(*camera, team->GetHealthBarTexture(), destroyedWorms);
        worm->Initialise(game.Context());
        auto* pointer = worm.get();
        team->AddWorm(std::move(worm));
        return pointer;
    }

    void CreateManager()
    {
        manager = std::make_unique<WormManager>(game.Context(), *camera, *weapon);
        manager->Initialise();
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

TEST_F(GameLifetime, SecondSceneOwnsIndependentPhysicsServicesAndCleanup)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    {
        GameScene second(game.Renderer());
        ASSERT_NO_THROW(second.Initialize());
        EXPECT_NE(&second.Context().physics, &game.Context().physics);
        EXPECT_NE(&second.Context().colliders, &game.Context().colliders);
        EXPECT_NE(&second.Context().contacts, &game.Context().contacts);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(second), subscriptions);
        const auto firstId = GameTestAccess::ActiveWorm(game);
        const auto secondId = GameTestAccess::Registry(second).TryGetComponent<Position>(firstId);
        ASSERT_TRUE(secondId);
        const auto extra = second.Context().contacts.AddEvent(firstId, BEGIN, [](b2Contact*) {});
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(second), subscriptions + 1);
        EXPECT_TRUE(second.Context().contacts.RemoveEvent(extra));
        EXPECT_NO_THROW(second.Update());
        EXPECT_NO_THROW(second.CleanUp());
        EXPECT_FALSE(GameTestAccess::HasResources(second));
    }
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_NO_THROW(game.Update());
}

TEST_F(GameLifetime, FailedSecondSceneStartupDoesNotInvalidateRunningPhysicsServices)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto active = GameTestAccess::ActiveWorm(game);
    GameScene second(game.Renderer());
    SceneAssetsWithout assets("powerBar.png");
    {
        ScopedWorkingDirectory directory(assets.Directory());
        EXPECT_THROW(second.Initialize(), SDL_Exception);
    }
    EXPECT_FALSE(GameTestAccess::HasResources(second));
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::ActiveWorm(game), active);
    EXPECT_NO_THROW(game.Update());
    EXPECT_NO_THROW(game.Render());
    ASSERT_NO_THROW(second.Initialize());
    EXPECT_EQ(GameTestAccess::SubscriptionCount(second), subscriptions);
    second.CleanUp();
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_NO_THROW(game.Update());
}

TEST_F(GameLifetime, CameraTargetLossIsDelayedConsumableAndResetByTracking)
{
    const auto id = GameTestAccess::ActiveWorm(game);
    camera->ChangeTarget(id);
    camera->Update();
    camera->ClearTarget();
    camera->Update();
    EXPECT_FALSE(camera->ConsumeTargetLost());
    SDL_Delay(1600);
    camera->Update();
    EXPECT_TRUE(camera->ConsumeTargetLost());
    EXPECT_FALSE(camera->ConsumeTargetLost());
    camera->Update();
    EXPECT_FALSE(camera->ConsumeTargetLost());
    camera->ChangeTarget(id);
    camera->Update();
    EXPECT_FALSE(camera->ConsumeTargetLost());
    camera->ClearTarget();
    camera->Update();
    EXPECT_FALSE(camera->ConsumeTargetLost());
    camera->CleanUp();
    EXPECT_FALSE(camera->ConsumeTargetLost());
}

TEST_F(GameLifetime, SceneConsumesCameraTargetLossAndChangesTeamExactlyOnce)
{
    ScopedDeltaTime delta(0);
    const auto first = GameTestAccess::ActiveWorm(game);
    camera->ChangeTarget(first);
    camera->Update();
    camera->ClearTarget();
    SDL_Delay(1600);
    game.Update();
    const auto next = GameTestAccess::ActiveWorm(game);
    EXPECT_NE(first, next);
    EXPECT_FALSE(camera->ConsumeTargetLost());
    game.Update();
    EXPECT_EQ(GameTestAccess::ActiveWorm(game), next);
}

TEST_F(GameLifetime, SceneDestructionReleasesGameplayButKeepsPlatformAlive)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    GameTestAccess::ResetScene(game);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
    EXPECT_TRUE(game.Objects().empty());
    EXPECT_NE(SDL_WasInit(SDL_INIT_VIDEO), 0);
    EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
    EXPECT_NE(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        SCOPED_TRACE(cycle);
        {
            GameScene scene(game.Renderer());
            scene.Initialize();
            EXPECT_EQ(GameTestAccess::SubscriptionCount(scene), subscriptions);
            EXPECT_THROW(scene.Initialize(), std::logic_error);
            EXPECT_NO_THROW(scene.Update());
            EXPECT_NO_THROW(scene.Render());
            EXPECT_NO_THROW(scene.RenderDebug());
            scene.QueueAdd(
                std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 2));
        }
        EXPECT_TRUE(game.Objects().empty());
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
        EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);
    }
}

TEST_F(GameLifetime, FailedSceneInitializationRollsBackAndCanBeRetried)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    GameTestAccess::ResetScene(game);
    GameScene scene(game.Renderer());
    {
        // The project root has no gameplay assets, so initialization fails after physics setup.
        ScopedWorkingDirectory directory(std::filesystem::current_path().parent_path());
        EXPECT_THROW(scene.Initialize(), SDL_Exception);
    }
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_TRUE(game.Objects().empty());
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
    EXPECT_NO_THROW(scene.CleanUp());
    EXPECT_NO_THROW(scene.Update());
    EXPECT_NO_THROW(scene.Render());
    EXPECT_NO_THROW(scene.RenderDebug());
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(GameTestAccess::SubscriptionCount(scene), subscriptions);
    scene.CleanUp();
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_NO_THROW(scene.CleanUp());
}

TEST_F(GameLifetime, SameSceneRestartsWithPendingEffectsAndResetsSessionState)
{
    auto& scene = game.Scene();
    auto* renderer = game.Renderer();
    const auto subscriptions = GameTestAccess::SubscriptionCount(scene);
    const auto firstWorm = GameTestAccess::ActiveWorm(game);
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        SCOPED_TRACE(cycle);
        GameTestAccess::ChangeTurn(game);
        EXPECT_NE(GameTestAccess::ActiveWorm(game), firstWorm);
        for (const auto& object : game.Objects())
        {
            if (auto* currentCamera = dynamic_cast<Camera*>(object.get()))
                currentCamera->ChangeZoom(2.f);
            if (auto* currentWeapon = dynamic_cast<Weapon*>(object.get()))
                WeaponTestAccess::SetCharge(*currentWeapon);
        }
        auto projectile = std::make_unique<Projectile>(100.f, 100.f, 0.f, 0.f);
        auto& activeProjectile = scene.AddObject(std::move(projectile));
        scene.AddObject(std::make_unique<ParticleSystem>("blood.png", 1.f, 0.f, 0.f, 4));
        scene.QueueAdd(std::make_unique<Projectile>(100.f, 100.f, 0.f, 0.f));
        scene.QueueAdd(std::make_unique<ParticleSystem>("blood.png", 1.f, 0.f, 0.f, 2));
        scene.RequestDestroy(activeProjectile);
        SDL_Event key{};
        for (auto code : {SDL_SCANCODE_D, SDL_SCANCODE_SPACE, SDL_SCANCODE_LSHIFT,
                          SDL_SCANCODE_RIGHT, SDL_SCANCODE_E})
        {
            key.key.keysym.scancode = code;
            Input::Get().UpdateInputsDown(key);
        }
        Time::deltaTime = 99.0;
        scene.CleanUp();
        EXPECT_FALSE(GameTestAccess::HasResources(scene));
        EXPECT_THROW(scene.Context(), std::logic_error);
        EXPECT_NO_THROW(scene.CleanUp());
        EXPECT_NO_THROW(scene.Update());
        EXPECT_NO_THROW(scene.Render());
        EXPECT_EQ(game.Renderer(), renderer);
        EXPECT_EQ(SDL_RenderClear(renderer), 0);
        EXPECT_NE(ImGui::GetCurrentContext(), nullptr);
        EXPECT_NE(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
        ASSERT_NO_THROW(scene.Initialize());
        EXPECT_DOUBLE_EQ(Time::deltaTime, 0.0);
        Time::UpdateFrameTime();
        EXPECT_DOUBLE_EQ(Time::deltaTime, 0.0);
        EXPECT_FLOAT_EQ(Input::Get().Horizontal(), 0.f);
        EXPECT_FLOAT_EQ(Input::Get().CameraHorizontal(), 0.f);
        EXPECT_FALSE(Input::Get().Jump());
        EXPECT_FALSE(Input::Get().UseAction());
        EXPECT_FALSE(Input::Get().CameraControll());
        EXPECT_EQ(Input::Get().ChangeWeapon(), 0);
        EXPECT_EQ(GameTestAccess::ActiveWorm(game), firstWorm);
        for (const auto& object : game.Objects())
        {
            if (auto* newCamera = dynamic_cast<Camera*>(object.get()))
            {
                EXPECT_FLOAT_EQ(newCamera->Zoom(), 1.f);
                EXPECT_FALSE(newCamera->ConsumeTargetLost());
            }
            if (auto* newWeapon = dynamic_cast<Weapon*>(object.get()))
                EXPECT_FLOAT_EQ(WeaponTestAccess::Charge(*newWeapon), 0.f);
        }
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(scene), subscriptions);
        EXPECT_EQ(game.PendingAddCount(), 0);
        EXPECT_EQ(game.PendingRemovalCount(), 0);
        EXPECT_NO_THROW(scene.Update());
        EXPECT_NO_THROW(scene.Render());
    }
}

TEST_F(GameLifetime, FailedGameStartupRollsBackPlatformAndCanBeRetried)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    game.Clean();
    for (const auto* missing : {"worms.png", "powerBar.png", "map.png", "Rick_Roll.ogg"})
    {
        SCOPED_TRACE(missing);
        {
            SceneAssetsWithout assets(missing);
            ScopedWorkingDirectory directory(assets.Directory());
            EXPECT_THROW(game.InitWindow("Failed startup", 800, 600), SDL_Exception);
        }
        EXPECT_FALSE(game.HasResources());
        EXPECT_FALSE(game.IsRunning());
        EXPECT_EQ(SDL_WasInit(0), 0u);
        EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
        EXPECT_EQ(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
        EXPECT_EQ(game.PendingAddCount(), 0);
        EXPECT_EQ(game.PendingRemovalCount(), 0);
        ASSERT_NO_THROW(game.InitWindow("Retry startup", 800, 600));
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
        EXPECT_NO_THROW(game.Update());
        EXPECT_NO_THROW(game.Render());
        game.Clean();
    }
}

TEST_F(GameLifetime, RejectedGameInitializationPreservesRunningSession)
{
    auto* renderer = game.Renderer();
    auto* scene = &game.Scene();
    EXPECT_THROW(game.InitWindow("Duplicate startup", 800, 600), std::logic_error);
    EXPECT_EQ(game.Renderer(), renderer);
    EXPECT_EQ(&game.Scene(), scene);
    EXPECT_TRUE(game.IsRunning());
    EXPECT_NO_THROW(game.Update());
    EXPECT_NO_THROW(game.Render());
}

TEST_F(GameLifetime, CleanupKeepsWorldsRendererAndSharedTexturesAliveForObjects)
{
    auto* texture = game.Registry().GetComponent<Sprite>(weapon->GetId()).texture;
    ASSERT_NE(texture, nullptr);
    auto& scene = game.Scene();
    const auto context = game.Context();
    auto stats = std::make_shared<QueueProbeStats>();
    auto pendingStats = std::make_shared<QueueProbeStats>();
    auto probe = std::make_unique<QueueProbe>(stats);
    probe->onCleanup = [&](QueueProbe& object)
    {
        EXPECT_THROW(scene.Context(), std::logic_error);
        EXPECT_EQ(context.physics.GetContactManager().m_contactListener, nullptr);
        EXPECT_EQ(SDL_QueryTexture(texture, nullptr, nullptr, nullptr, nullptr), 0);
        EXPECT_EQ(SDL_RenderClear(context.renderer), 0);
        EXPECT_NE(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
        auto id = context.world.CreateEntity();
        context.world.DestroyEntity(id);
        EXPECT_THROW(object.Owner().QueueAdd(std::make_unique<QueueProbe>(pendingStats)),
                     std::logic_error);
        EXPECT_THROW(object.Owner().RequestDestroy(object), std::logic_error);
    };
    game.AddObject(std::move(probe));
    game.QueueAdd(std::make_unique<QueueProbe>(pendingStats));
    game.Clean();
    EXPECT_EQ(stats->cleaned, 1);
    EXPECT_EQ(stats->destroyed, 1);
    EXPECT_EQ(pendingStats->initialized, 0);
    EXPECT_EQ(pendingStats->cleaned, 2);
    EXPECT_EQ(pendingStats->destroyed, 2);
    EXPECT_FALSE(game.HasResources());
}

TEST_F(GameLifetime, GameplayConstructorsDoNotAllocateEntitiesBodiesOrSubscriptions)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto pending = game.PendingAddCount();
    SceneAssetsWithout assets("worms.png");
    ScopedWorkingDirectory directory(assets.Directory());
    Worm worm(*camera, nullptr, {-1.f, 2.f});
    HealthBar bar(camera->GetId(), *camera, 100, nullptr);
    FocusPoint focus;
    Weapon localWeapon(*camera);
    WeaponManager weapons(*game.Renderer(), localWeapon);
    WormManager worms(game.Context(), *camera, localWeapon);
    WormTeam emptyTeam(nullptr);
    const auto cameraX = camera->X();
    EXPECT_NO_THROW(worms.Update());
    EXPECT_NO_THROW(worms.CleanUp());
    EXPECT_FLOAT_EQ(camera->X(), cameraX);
    EXPECT_FALSE(camera->ConsumeTargetLost());
    EXPECT_FALSE(worm.HasEntity());
    EXPECT_FALSE(bar.HasEntity());
    EXPECT_FALSE(focus.HasEntity());
    EXPECT_FALSE(localWeapon.HasEntity());
    EXPECT_THROW(worms.CreateTeam(1), std::logic_error);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_EQ(game.PendingAddCount(), pending);
}

TEST_F(GameLifetime, WormInitializationFailureCanBeRetriedWithTheSameObject)
{
    CreateTestTeam();
    auto incomplete = std::make_unique<World>(game.Renderer());
    incomplete->RegisterComponent<Position>();
    incomplete->RegisterComponent<Sprite>();
    incomplete->RegisterComponent<Health>();
    incomplete->RegisterComponent<RigidBody>();
    const auto available = incomplete->GetAmountOfAvailableEntities();
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    Worm worm(*camera, team->GetHealthBarTexture(), {3.f, 4.f});
    EXPECT_THROW(worm.Initialise(game.ContextWith(*incomplete)), std::exception);
    EXPECT_FALSE(worm.HasEntity());
    EXPECT_EQ(incomplete->GetAmountOfAvailableEntities(), available);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    ASSERT_NO_THROW(worm.Initialise(game.Context()));
    EXPECT_TRUE(worm.HasEntity());
    EXPECT_FLOAT_EQ(game.Registry().GetComponent<Position>(worm.GetId()).x, 3.f);
    EXPECT_FLOAT_EQ(game.Registry().GetComponent<Position>(worm.GetId()).y, 4.f);
    const auto id = worm.GetId();
    EXPECT_THROW(worm.Initialise(game.Context()), std::logic_error);
    EXPECT_EQ(worm.GetId(), id);
    worm.CleanUp();
    ASSERT_NO_THROW(worm.Initialise(game.Context()));
    EXPECT_FLOAT_EQ(game.Registry().GetComponent<Position>(worm.GetId()).x, 3.f);
    EXPECT_FLOAT_EQ(game.Registry().GetComponent<Position>(worm.GetId()).y, 4.f);
    worm.CleanUp();
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
}

TEST_F(GameLifetime, PhysicsObjectsRejectForeignWorldBeforeAllocatingResources)
{
    CreateTestTeam();
    b2World foreignPhysics({0, 0});
    const SceneContext foreign{game.Renderer(), game.Registry(),          foreignPhysics,
                               game.Scene(),    game.Context().colliders, game.Context().contacts};
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    Worm worm(*camera, team->GetHealthBarTexture(), {3.f, 4.f});
    Projectile projectile(0, 2, 0, 0);
    Map map;
    EXPECT_THROW(worm.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(projectile.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(map.Initialise(foreign), std::invalid_argument);
    EXPECT_FALSE(worm.HasEntity());
    EXPECT_FALSE(projectile.HasEntity());
    EXPECT_FALSE(map.HasEntity());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(foreignPhysics.GetBodyCount(), 0);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    ASSERT_NO_THROW(worm.Initialise(game.Context()));
    ASSERT_NO_THROW(projectile.Initialise(game.Context()));
    ASSERT_NO_THROW(map.Initialise(game.Context()));
    worm.CleanUp();
    projectile.CleanUp();
    map.CleanUp();
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
}

TEST_F(GameLifetime, PhysicsWorldValidationPrecedesEntityAllocation)
{
    auto exhausted = std::make_unique<World>(game.Renderer());
    while (exhausted->GetAmountOfAvailableEntities())
        exhausted->CreateEntity();
    b2World foreignPhysics({0, 0});
    const SceneContext foreign{game.Renderer(),          *exhausted,
                               foreignPhysics,           game.Scene(),
                               game.Context().colliders, game.Context().contacts};
    Worm worm(*camera, nullptr, {0.f, 2.f});
    Projectile projectile(0, 2, 0, 0);
    Map map;
    // Entity allocation would throw runtime_error, not invalid_argument.
    EXPECT_THROW(worm.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(projectile.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(map.Initialise(foreign), std::invalid_argument);
    EXPECT_EQ(foreignPhysics.GetBodyCount(), 0);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, PhysicsObjectsRejectMismatchedContactManagerBeforeInitialization)
{
    ContactManager foreignContacts;
    const SceneContext foreign{game.Renderer(), game.Registry(),          game.Physics(),
                               game.Scene(),    game.Context().colliders, foreignContacts};
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    Worm worm(*camera, nullptr, {0.f, 2.f});
    Projectile projectile(0, 2, 0, 0);
    Map map;
    EXPECT_THROW(worm.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(projectile.Initialise(foreign), std::invalid_argument);
    EXPECT_THROW(map.Initialise(foreign), std::invalid_argument);
    EXPECT_FALSE(worm.HasEntity());
    EXPECT_FALSE(projectile.HasEntity());
    EXPECT_FALSE(map.HasEntity());
    EXPECT_EQ(foreignContacts.GetSubscriptionCount(), 0);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, TeamRejectsWormsBeforeInitializationAndAfterCleanupWithoutLeaks)
{
    WormTeam::TexturePtr texture(SDL_CreateTexture(game.Renderer(), SDL_PIXELFORMAT_RGBA8888,
                                                   SDL_TEXTUREACCESS_STATIC, 40, 10));
    ASSERT_TRUE(texture);
    team = std::make_unique<WormTeam>(std::move(texture));
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto reject = [this, subscriptions]
    {
        auto worm =
            std::make_unique<TrackedWorm>(*camera, team->GetHealthBarTexture(), destroyedWorms);
        worm->Initialise(game.Context());
        EXPECT_THROW(team->AddWorm(std::move(worm)), std::logic_error);
        EXPECT_EQ(team->Size(), 0);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    };
    reject();
    EXPECT_EQ(destroyedWorms, 1);
    ASSERT_NO_THROW(team->Initialise());
    EXPECT_THROW(team->Initialise(), std::logic_error);
    EXPECT_THROW(team->AddWorm(nullptr), std::invalid_argument);
    EXPECT_THROW(
        team->AddWorm(std::make_unique<Worm>(*camera, team->GetHealthBarTexture(), Position{0, 2})),
        std::invalid_argument);
    AddTrackedWorm();
    EXPECT_EQ(team->Size(), 1);
    team->CleanUp();
    reject();
    EXPECT_EQ(destroyedWorms, 3);
    ASSERT_NO_THROW(team->Initialise());
    AddTrackedWorm();
    EXPECT_EQ(team->Size(), 1);
    team->CleanUp();
    EXPECT_EQ(destroyedWorms, 4);
}

TEST_F(GameLifetime, FailedTeamCreationDoesNotConsumeSpawnPositions)
{
    auto incomplete = std::make_unique<World>(game.Renderer());
    incomplete->RegisterComponent<Position>();
    incomplete->RegisterComponent<Sprite>();
    incomplete->RegisterComponent<Health>();
    incomplete->RegisterComponent<RigidBody>();
    WormManager local(game.ContextWith(*incomplete), *camera, *weapon);
    local.Initialise();
    EXPECT_THROW(local.CreateTeam(2), std::exception);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    incomplete->RegisterComponent<Follow>();
    ASSERT_NO_THROW(local.CreateTeam(2));
    EXPECT_FLOAT_EQ(incomplete->GetComponent<Position>(local.GetActiveWormId()).x, -1.f);
    EXPECT_FLOAT_EQ(game.Physics().GetBodyList()->GetPosition().x, 0.f);
    local.CreateTeam(1);
    EXPECT_FLOAT_EQ(game.Physics().GetBodyList()->GetPosition().x, 1.f);
    local.CleanUp();
    local.Initialise();
    local.CreateTeam(1);
    EXPECT_FLOAT_EQ(incomplete->GetComponent<Position>(local.GetActiveWormId()).x, -1.f);
    local.CleanUp();
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
}

TEST_F(GameLifetime, RestartedGameRestoresInitialWormSpawnPosition)
{
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        const auto wormId = GameTestAccess::ActiveWorm(game);
        const auto& position = game.Registry().GetComponent<Position>(wormId);
        EXPECT_FLOAT_EQ(position.x, -1.f);
        EXPECT_FLOAT_EQ(position.y, 2.f);
        game.Clean();
        ASSERT_NO_THROW(game.InitWindow("Spawn restart test", 800, 600));
    }
}

TEST_F(GameLifetime, HealthBarInitializationFailurePreservesConfigurationForRetry)
{
    CreateTestTeam();
    auto incomplete = std::make_unique<World>(game.Renderer());
    incomplete->RegisterComponent<Position>();
    incomplete->RegisterComponent<Health>();
    HealthBar bar(camera->GetId(), *camera, 75, team->GetHealthBarTexture());
    const auto available = incomplete->GetAmountOfAvailableEntities();
    EXPECT_THROW(bar.Initialise(game.ContextWith(*incomplete)), std::exception);
    EXPECT_FALSE(bar.HasEntity());
    EXPECT_EQ(incomplete->GetAmountOfAvailableEntities(), available);
    ASSERT_NO_THROW(bar.Initialise(game.Context()));
    EXPECT_EQ(bar.getCurrentHp(), 75);
    EXPECT_THROW(bar.Initialise(game.Context()), std::logic_error);
    bar.CleanUp();
    ASSERT_NO_THROW(bar.Initialise(game.Context()));
    EXPECT_EQ(bar.getCurrentHp(), 75);
    bar.CleanUp();
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, ObjectContextIsCopiedAndUnavailableOutsideItsLifetime)
{
    QueueProbe probe(std::make_shared<QueueProbeStats>());
    EXPECT_THROW(probe.Owner(), std::logic_error);
    probe.Initialise(game.ContextWith(game.Registry()));
    EXPECT_EQ(&probe.Owner(), &game.Scene());
    probe.CleanUp();
    EXPECT_THROW(probe.Owner(), std::logic_error);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, SceneRejectsInitializedObjectsWithForeignCommands)
{
    GameScene foreign(game.Renderer());
    auto object = std::make_unique<GameObject>();
    object->Initialise(SceneContext{game.Renderer(), game.Registry(), game.Physics(), foreign,
                                    game.Context().colliders, game.Context().contacts});
    EXPECT_THROW(game.AddObject(std::move(object)), std::invalid_argument);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_THROW(foreign.Context(), std::logic_error);
}

TEST_F(GameLifetime, SceneRejectsInitializedObjectsWithDifferentPhysicsAndCleansThem)
{
    b2World foreignPhysics({0, 0});
    auto object = std::make_unique<GameObject>();
    object->Initialise(SceneContext{game.Renderer(), game.Registry(), foreignPhysics, game.Scene(),
                                    game.Context().colliders, game.Context().contacts});
    EXPECT_THROW(game.AddObject(std::move(object)), std::invalid_argument);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.PendingAddCount(), 0);
}

TEST_F(GameLifetime, QueueRejectionCleansAnAlreadyInitializedObject)
{
    auto object = std::make_unique<GameObject>();
    object->Initialise(game.Context());
    EXPECT_THROW(game.QueueAdd(std::move(object)), std::invalid_argument);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.PendingAddCount(), 0);
}

TEST_F(GameLifetime, ObjectsSpawnedDuringUpdateWaitUntilNextFrameAndSelfRemovalIsDeferred)
{
    ScopedDeltaTime delta(0);
    const auto active = game.Objects().size();
    auto parentStats = std::make_shared<QueueProbeStats>();
    auto childStats = std::make_shared<QueueProbeStats>();
    auto parent = std::make_unique<QueueProbe>(parentStats);
    parent->onUpdate = [this, childStats](QueueProbe& object)
    {
        object.Owner().QueueAdd(std::make_unique<QueueProbe>(childStats));
        object.Owner().RequestDestroy(object);
        object.Owner().RequestDestroy(object);
        EXPECT_THROW(game.AddObject(std::make_unique<GameObject>()), std::logic_error);
    };
    auto& added = game.AddObject(std::move(parent));
    const auto handle = game.Registry().GetHandle(added.GetId());
    ASSERT_TRUE(handle);
    game.Update();
    EXPECT_EQ(parentStats->updated, 1);
    EXPECT_EQ(parentStats->destroyed, 0);
    EXPECT_EQ(childStats->initialized, 0);
    EXPECT_EQ(game.PendingAddCount(), 1);
    EXPECT_EQ(game.PendingRemovalCount(), 1);
    game.Update();
    EXPECT_EQ(parentStats->cleaned, 1);
    EXPECT_EQ(parentStats->destroyed, 1);
    EXPECT_FALSE(game.Registry().IsAlive(*handle));
    EXPECT_EQ(childStats->initialized, 1);
    EXPECT_EQ(childStats->updated, 1);
    EXPECT_EQ(game.Objects().size(), active + 1);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
}

TEST_F(GameLifetime, InitializerCanQueueAnotherObjectWithoutChangingItsCurrentBatch)
{
    ScopedDeltaTime delta(0);
    auto parentStats = std::make_shared<QueueProbeStats>();
    auto childStats = std::make_shared<QueueProbeStats>();
    auto parent = std::make_unique<QueueProbe>(parentStats);
    parent->onInitialize = [this, childStats](QueueProbe& object)
    {
        object.Owner().QueueAdd(std::make_unique<QueueProbe>(childStats));
        EXPECT_THROW(game.AddObject(std::make_unique<GameObject>()), std::logic_error);
    };
    game.QueueAdd(std::move(parent));
    game.Update();
    EXPECT_EQ(parentStats->initialized, 1);
    EXPECT_EQ(childStats->initialized, 0);
    EXPECT_EQ(game.PendingAddCount(), 1);
    game.Update();
    EXPECT_EQ(parentStats->initialized, 1);
    EXPECT_EQ(childStats->initialized, 1);
    EXPECT_EQ(game.PendingAddCount(), 0);
}

TEST_F(GameLifetime, FailedMiddleAdditionDoesNotReinitializeEarlierObjectsOrLoseLaterOnes)
{
    ScopedDeltaTime delta(0);
    const auto active = game.Objects().size();
    auto first = std::make_shared<QueueProbeStats>();
    auto failed = std::make_shared<QueueProbeStats>();
    auto last = std::make_shared<QueueProbeStats>();
    game.QueueAdd(std::make_unique<QueueProbe>(first));
    auto failing = std::make_unique<QueueProbe>(failed);
    failing->onInitialize = [](QueueProbe& object)
    {
        object.Owner().RequestDestroy(object);
        throw std::runtime_error("Initializer failure");
    };
    game.QueueAdd(std::move(failing));
    game.QueueAdd(std::make_unique<QueueProbe>(last));
    EXPECT_THROW(game.Update(), std::runtime_error);
    EXPECT_EQ(first->initialized, 1);
    EXPECT_EQ(first->destroyed, 0);
    EXPECT_EQ(failed->cleaned, 1);
    EXPECT_EQ(failed->destroyed, 1);
    EXPECT_EQ(last->initialized, 0);
    EXPECT_EQ(game.Objects().size(), active + 1);
    EXPECT_EQ(game.PendingAddCount(), 1);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 1);
    EXPECT_NO_THROW(game.Update());
    EXPECT_EQ(first->initialized, 1);
    EXPECT_EQ(last->initialized, 1);
    EXPECT_EQ(game.Objects().size(), active + 2);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 2);
}

TEST_F(GameLifetime, PendingObjectCanBeCancelledBeforeInitialization)
{
    auto stats = std::make_shared<QueueProbeStats>();
    auto object = std::make_unique<QueueProbe>(stats);
    auto* pointer = object.get();
    game.QueueAdd(std::move(object));
    game.RequestDestroy(*pointer);
    game.RequestDestroy(*pointer);
    EXPECT_EQ(game.PendingRemovalCount(), 1);
    game.Update();
    EXPECT_EQ(stats->initialized, 0);
    EXPECT_EQ(stats->destroyed, 1);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, QueueRejectsNullObjectsAndRemovalOfObjectsOutsideScene)
{
    GameObject foreign;
    const auto active = game.Objects().size();
    EXPECT_THROW(game.QueueAdd(nullptr), std::invalid_argument);
    EXPECT_THROW(game.AddObject(nullptr), std::invalid_argument);
    EXPECT_THROW(game.RequestDestroy(foreign), std::invalid_argument);
    GameScene inactive(game.Renderer());
    EXPECT_THROW(inactive.QueueAdd(std::make_unique<GameObject>()), std::logic_error);
    EXPECT_THROW(inactive.RequestDestroy(*camera), std::logic_error);
    EXPECT_EQ(game.Objects().size(), active);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
}

TEST_F(GameLifetime, RemovalCleanupFailurePreservesOwnershipAndRemainingRequestsForRetry)
{
    ScopedDeltaTime delta(0);
    auto firstStats = std::make_shared<QueueProbeStats>();
    auto lastStats = std::make_shared<QueueProbeStats>();
    auto first = std::make_unique<QueueProbe>(firstStats);
    first->onCleanup = [attempt = 0](QueueProbe&) mutable
    {
        if (++attempt == 1) throw std::runtime_error("Cleanup failure");
    };
    auto& firstObject = game.AddObject(std::move(first));
    auto& lastObject = game.AddObject(std::make_unique<QueueProbe>(lastStats));
    game.RequestDestroy(firstObject);
    game.RequestDestroy(lastObject);
    EXPECT_THROW(game.Update(), std::runtime_error);
    EXPECT_EQ(firstStats->destroyed, 0);
    EXPECT_EQ(lastStats->cleaned, 0);
    EXPECT_EQ(game.PendingRemovalCount(), 2);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 2);
    EXPECT_NO_THROW(game.Update());
    EXPECT_EQ(firstStats->cleaned, 2);
    EXPECT_EQ(firstStats->destroyed, 1);
    EXPECT_EQ(lastStats->cleaned, 1);
    EXPECT_EQ(lastStats->destroyed, 1);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, RemovalRequestedFromCleanupWaitsForNextFrame)
{
    auto firstStats = std::make_shared<QueueProbeStats>();
    auto lastStats = std::make_shared<QueueProbeStats>();
    auto& lastObject = game.AddObject(std::make_unique<QueueProbe>(lastStats));
    auto first = std::make_unique<QueueProbe>(firstStats);
    first->onCleanup = [&lastObject](QueueProbe& object)
    {
        object.Owner().RequestDestroy(lastObject);
    };
    auto& firstObject = game.AddObject(std::move(first));
    game.RequestDestroy(firstObject);
    game.Update();
    EXPECT_EQ(firstStats->destroyed, 1);
    EXPECT_EQ(lastStats->destroyed, 0);
    EXPECT_EQ(game.PendingRemovalCount(), 1);
    game.Update();
    EXPECT_EQ(lastStats->destroyed, 1);
    EXPECT_EQ(game.PendingRemovalCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
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

    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    int cleaned = 0;
    int destroyed = 0;
    for (int failure : {1, 0})
    {
        auto object = std::make_unique<CleanupObject>(failure, cleaned, destroyed);
        object->Initialise(game.Context());
        game.AddObject(std::move(object));
    }
    for (int failure : {2, 0})
        game.QueueAdd(
            std::make_unique<CleanupObject>(failure, cleaned, destroyed));

    EXPECT_NO_THROW(GameTestAccess::ResetScene(game));
    EXPECT_EQ(cleaned, 4);
    EXPECT_EQ(destroyed, 4);
    EXPECT_TRUE(game.Objects().empty());
    EXPECT_TRUE(game.PendingAddCount() == 0);
    EXPECT_TRUE(game.PendingRemovalCount() == 0);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
    EXPECT_FALSE(GameTestAccess::TakePendingException(game));
    EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);

    GameScene next(game.Renderer());
    ASSERT_NO_THROW(next.Initialize());
    EXPECT_EQ(GameTestAccess::Registry(next).GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(GameTestAccess::Physics(next).GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(next), subscriptions);
}

TEST_F(GameLifetime, FailedStartupObjectRemainsAliveWhileItsDependentsAreCleaned)
{
    auto failedStats = std::make_shared<QueueProbeStats>();
    auto observerStats = std::make_shared<QueueProbeStats>();
    auto failed = std::make_unique<QueueProbe>(failedStats);
    failed->onInitialize = [](QueueProbe&) { throw std::runtime_error("Startup failure"); };
    auto observer = std::make_unique<QueueProbe>(observerStats);
    observer->onCleanup = [failedStats](QueueProbe&)
    {
        EXPECT_EQ(failedStats->destroyed, 0);
    };
    game.AddObject(std::move(observer));
    EXPECT_THROW(GameTestAccess::ActivateStartupObject(game.Scene(), std::move(failed)),
                 std::runtime_error);
    EXPECT_EQ(failedStats->initialized, 1);
    EXPECT_EQ(failedStats->destroyed, 0);
    EXPECT_NO_THROW(game.Clean());
    EXPECT_EQ(observerStats->cleaned, 1);
    EXPECT_EQ(observerStats->destroyed, 1);
    EXPECT_EQ(failedStats->destroyed, 1);
    EXPECT_FALSE(game.HasResources());
    EXPECT_NO_THROW(game.Clean());
}

TEST_F(GameLifetime, MissingWeaponPowerBarRollsBackWithoutDanglingManagerReferences)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    GameTestAccess::ResetScene(game);
    GameScene scene(game.Renderer());
    SceneAssetsWithout assets("powerBar.png");
    {
        ScopedWorkingDirectory directory(assets.Directory());
        try
        {
            scene.Initialize();
            FAIL() << "Missing power bar should fail weapon initialization";
        }
        catch (const SDL_Exception& error)
        {
            EXPECT_EQ(std::filesystem::path(error.GetFile()).filename(), "Weapon.cpp");
        }
    }
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
    EXPECT_FALSE(GameTestAccess::TakePendingException(game));
    EXPECT_THROW(scene.Context(), std::logic_error);
    EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);
    EXPECT_NO_THROW(scene.CleanUp());
    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(GameTestAccess::Registry(scene).GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(GameTestAccess::Physics(scene).GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(scene), subscriptions);
    EXPECT_NO_THROW(scene.CleanUp());
    EXPECT_FALSE(GameTestAccess::HasResources(scene));
}

TEST_F(GameLifetime, MissingMusicRollsBackFullyConstructedGameplayAndAllowsRetry)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    GameTestAccess::ResetScene(game);
    GameScene scene(game.Renderer());
    SceneAssetsWithout assets("Rick_Roll.ogg");
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
    EXPECT_TRUE(game.Objects().empty());
    EXPECT_TRUE(game.PendingAddCount() == 0);
    EXPECT_TRUE(game.PendingRemovalCount() == 0);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
    EXPECT_FALSE(GameTestAccess::TakePendingException(game));
    EXPECT_EQ(Mix_PlayingMusic(), 0);
    EXPECT_EQ(SDL_RenderClear(game.Renderer()), 0);
    EXPECT_NO_THROW(scene.CleanUp());

    ASSERT_NO_THROW(scene.Initialize());
    EXPECT_EQ(GameTestAccess::Registry(scene).GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(GameTestAccess::Physics(scene).GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(scene), subscriptions);
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
    manager->OnCameraTargetLost();
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
    EXPECT_NO_THROW(manager->OnCameraTargetLost());
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
    WormManager failing(game.ContextWith(*incompleteWorld), *camera, *weapon);
    failing.Initialise();
    EXPECT_THROW(failing.CreateTeam(2), std::exception);
    EXPECT_EQ(incompleteWorld->GetAmountOfAvailableEntities(), available);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_THROW(failing.GetActiveWormId(), std::logic_error);
    failing.CleanUp();
}

TEST_F(GameLifetime, FailedTextureLoadCanBeCleanedWithoutLeakingAnEntity)
{
    game.QueueAdd(std::make_unique<ParticleSystem>(
        "missing-lifetime-test-texture.png", 1, 0, 0, 10));
    EXPECT_THROW(game.Update(), SDL_Exception);
    EXPECT_EQ(game.PendingAddCount(), 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_NO_THROW(game.Update());
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
    EXPECT_THROW(effect.Initialise(game.ContextWith(incompleteWorld)), std::exception);
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
    game.AddObject(std::move(projectile));

    auto particles = std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 10);
    auto* particlesPointer = particles.get();
    game.AddObject(std::move(particles));
    game.QueueAdd(std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 3));
    game.RequestDestroy(*projectilePointer);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 1);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 12);

    EXPECT_NO_THROW(game.Clean());
    EXPECT_FALSE(game.HasResources());
    EXPECT_TRUE(game.Objects().empty());
    EXPECT_TRUE(game.PendingAddCount() == 0);
    EXPECT_TRUE(game.PendingRemovalCount() == 0);
    EXPECT_EQ(SDL_WasInit(0), 0u);
    EXPECT_EQ(ImGui::GetCurrentContext(), nullptr);
    EXPECT_EQ(Mix_QuerySpec(nullptr, nullptr, nullptr), 0);
    EXPECT_NO_THROW(game.Clean());
}

TEST_F(GameLifetime, ProjectileCanUnsubscribeDuringCollisionWithoutRemovingOtherListeners)
{
    auto projectile = std::make_unique<Projectile>(0, 2, 0, 0);
    auto* pointer = projectile.get();
    game.AddObject(std::move(projectile));
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
    auto observer = game.Context().contacts.AddEvent(id, BEGIN, [&](b2Contact*) { ++calls; });
    game.Physics().Step(1.f / 60, 8, 3);
    ASSERT_NE(body->GetContactList(), nullptr);
    const int previousCalls = calls;
    game.Context().contacts.BeginContact(body->GetContactList()->contact);
    EXPECT_EQ(calls, previousCalls + 1);
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_NO_THROW(pointer->CleanUp());
    EXPECT_TRUE(game.Context().contacts.RemoveEvent(observer));
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
    game.Context().contacts.AddEvent(info.id, BEGIN,
                                     [&](b2Contact*)
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
    EXPECT_FALSE(GameTestAccess::TakePendingException(game));
    ASSERT_NE(moving->GetContactList(), nullptr);
    EXPECT_NO_THROW(game.Context().contacts.BeginContact(moving->GetContactList()->contact));
    EXPECT_EQ(calls, 2);
    EXPECT_NO_THROW(game.Clean());
    EXPECT_FALSE(game.HasResources());
    EXPECT_FALSE(GameTestAccess::TakePendingException(game));
    EXPECT_NO_THROW(game.Clean());
}

TEST_F(GameLifetime, EmptyTerrainRebuildRemovesBodyAndCleanupCanBeRepeated)
{
    Map* map = nullptr;
    for (const auto& object : game.Objects())
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
    for (const auto& object : game.Objects())
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
    for (const auto& object : game.Objects())
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
    for (const auto& object : game.Objects())
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    const auto id = map->GetId();
    auto* oldTexture = game.Registry().GetComponent<Sprite>(id).texture;
    auto* oldBody = game.Registry().GetComponent<RigidBody>(id).body;
    MapTestAccess::RequestRebuild(*map);
    EXPECT_THROW(MapTestAccess::RebuildWithoutPosition(*map), std::exception);
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
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    Map map;
    // Missing Sprite fails after loading the surface, texture and callback.
    EXPECT_THROW(map.Initialise(game.ContextWith(incompleteWorld)), std::exception);
    EXPECT_FALSE(map.HasEntity());
    EXPECT_FALSE(MapTestAccess::HasResources(map));
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), entities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_NO_THROW(map.CleanUp());
    EXPECT_NO_THROW(map.Update());
    EXPECT_NO_THROW(map.DestroyMapAtLocalPoint({0, 0}));
}

TEST_F(GameLifetime, FailedProjectileInitializationAutomaticallyReleasesBodyAndSubscription)
{
    Camera uninitializedCamera;
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    Projectile projectile(0, 2, 0, 0);
    projectile.SetCamera(&uninitializedCamera);
    // ChangeTarget fails after creating the projectile's body and subscription.
    EXPECT_THROW(projectile.Initialise(game.Context()), std::logic_error);
    EXPECT_FALSE(projectile.HasEntity());
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
    EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
    EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    EXPECT_NO_THROW(projectile.CleanUp());
    EXPECT_NO_THROW(projectile.Update());
}

TEST_F(GameLifetime, FailedCameraInitializationAutomaticallyReleasesItsEntity)
{
    World incompleteWorld(game.Renderer());
    const auto available = incompleteWorld.GetAmountOfAvailableEntities();
    Camera localCamera;
    EXPECT_THROW(localCamera.Initialise(game.ContextWith(incompleteWorld)), std::exception);
    EXPECT_FALSE(localCamera.HasEntity());
    EXPECT_EQ(incompleteWorld.GetAmountOfAvailableEntities(), available);
    EXPECT_NO_THROW(localCamera.CleanUp());
    EXPECT_NO_THROW(localCamera.Update());
}

TEST_F(GameLifetime, FailedWeaponTextureLoadAutomaticallyReleasesItsEntity)
{
    Weapon localWeapon(*camera);
    EXPECT_THROW(localWeapon.Initialise(game.ContextWith(nullptr)), SDL_Exception);
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
    HealthBar bar(camera->GetId(), *camera, 100, texture.get());
    bar.Initialise(game.Context());
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
    localWeapon.Initialise(game.Context());
    localWeapon.SetTexture(texture.get());
    localWeapon.SetProjectileTexture(texture.get());
    Projectile projectile(0, 2, 0, 0);
    projectile.SetTexture(texture.get());
    projectile.Initialise(game.Context());
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
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    for (int i = 0; i < 5; ++i)
    {
        ParticleSystem particles("blood.png", 1, 0, 0, 4);
        particles.Initialise(game.Context());
        EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(particles), 1);
        const auto particleId = particles.GetId();
        EXPECT_THROW(particles.Initialise(game.Context()), std::logic_error);
        EXPECT_EQ(particles.GetId(), particleId);
        particles.CleanUp();
        EXPECT_EQ(ParticleSystemTestAccess::OwnedResources(particles), 0);
        EXPECT_EQ(ParticleSystemTestAccess::ParticleCount(particles), 0);
        EXPECT_NO_THROW(particles.CleanUp());
        EXPECT_NO_THROW(particles.Update());

        Camera localCamera;
        localCamera.Initialise(game.Context());
        const auto cameraId = localCamera.GetId();
        EXPECT_THROW(localCamera.Initialise(game.Context()), std::logic_error);
        EXPECT_EQ(localCamera.GetId(), cameraId);
        localCamera.CleanUp();
        EXPECT_NO_THROW(localCamera.CleanUp());
        EXPECT_NO_THROW(localCamera.Update());

        Weapon localWeapon(*camera);
        localWeapon.Initialise(game.Context());
        EXPECT_EQ(WeaponTestAccess::OwnedResources(localWeapon), 1);
        const auto weaponId = localWeapon.GetId();
        EXPECT_THROW(localWeapon.Initialise(game.Context()), std::logic_error);
        EXPECT_EQ(localWeapon.GetId(), weaponId);
        localWeapon.CleanUp();
        EXPECT_EQ(WeaponTestAccess::OwnedResources(localWeapon), 0);
        EXPECT_NO_THROW(localWeapon.CleanUp());

        Projectile projectile(0, 2, 0, 0);
        projectile.Initialise(game.Context());
        const auto projectileId = projectile.GetId();
        EXPECT_THROW(projectile.Initialise(game.Context()), std::logic_error);
        EXPECT_EQ(projectile.GetId(), projectileId);
        projectile.CleanUp();
        EXPECT_NO_THROW(projectile.CleanUp());

        Map map;
        map.Initialise(game.Context());
        EXPECT_EQ(MapTestAccess::OwnedResources(map), 2);
        const auto mapId = map.GetId();
        EXPECT_THROW(map.Initialise(game.Context()), std::logic_error);
        EXPECT_EQ(map.GetId(), mapId);
        map.CleanUp();
        EXPECT_EQ(MapTestAccess::OwnedResources(map), 0);
        EXPECT_NO_THROW(map.CleanUp());

        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
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
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto active = game.Objects().size();
    auto destroyed = std::make_shared<int>(0);
    for (int cycle = 0; cycle < 10; ++cycle)
    {
        SCOPED_TRACE(cycle);
        auto effect = std::make_unique<ExpiringParticles>(destroyed);
        auto* pointer = effect.get();
        game.QueueAdd(std::move(effect));
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
        EXPECT_EQ(game.PendingRemovalCount(), 1);
        game.Update();
        EXPECT_FALSE(game.Registry().IsAlive(*handle));
        EXPECT_EQ(*destroyed, cycle + 1);
        EXPECT_EQ(game.Objects().size(), active);
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    }
}

TEST_F(GameLifetime, ExplosionAndParticleExpiryRestoreCountsAcrossRepeatedGameUpdates)
{
    ScopedDeltaTime delta(0);
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto active = game.Objects().size();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        auto projectile = std::make_unique<Projectile>(100, -20, 0, 0);
        projectile->SetExplosionRadius(0.5f);
        projectile->Initialise(game.Context());
        const auto handle = game.Registry().GetHandle(projectile->GetId());
        ASSERT_TRUE(handle);
        game.AddObject(std::move(projectile));
        game.Update();
        EXPECT_EQ(game.PendingAddCount(), 1);
        EXPECT_EQ(game.PendingRemovalCount(), 1);
        game.Update();
        EXPECT_FALSE(game.Registry().IsAlive(*handle));
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 101);
        for (const auto& object : game.Objects())
            if (auto* particles = dynamic_cast<ParticleSystem*>(object.get()))
            {
                ScopedDeltaTime expire(6);
                particles->Update();
            }
        game.Update();
        EXPECT_EQ(game.Objects().size(), active);
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    }
}

TEST_F(GameLifetime, RepeatedLastTeamDeathsAndTheirParticlesRestoreCounts)
{
    ScopedDeltaTime delta(0);
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    const auto active = game.Objects().size();
    CreateManager();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        manager->CreateTeam(1);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities - 3);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies + 1);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions + 3);
        KillActiveWorm();
        EXPECT_THROW(manager->GetActiveWormId(), std::logic_error);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
        EXPECT_EQ(game.PendingAddCount(), 1);
        game.Update();
        for (const auto& object : game.Objects())
            if (auto* particles = dynamic_cast<ParticleSystem*>(object.get()))
            {
                ScopedDeltaTime expire(6);
                particles->Update();
            }
        game.Update();
        EXPECT_EQ(game.Objects().size(), active);
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
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
    object->Initialise(game.Context());
    const auto handle = game.Registry().GetHandle(object->GetId());
    auto* pointer = object.get();
    game.AddObject(std::move(object));
    game.RequestDestroy(*pointer);
    game.RequestDestroy(*pointer);
    game.Update();
    EXPECT_EQ(cleaned, 1);
    EXPECT_EQ(destroyed, 1);
    EXPECT_FALSE(game.Registry().IsAlive(*handle));
    EXPECT_TRUE(game.PendingRemovalCount() == 0);
    EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
}

TEST_F(GameLifetime, RepeatedTerrainDeformationReplacesResourcesWithoutGrowingCounts)
{
    Map* map = nullptr;
    for (const auto& object : game.Objects())
        if (auto* candidate = dynamic_cast<Map*>(object.get())) map = candidate;
    ASSERT_NE(map, nullptr);
    auto* surface = MapTestAccess::Surface(*map);
    ASSERT_EQ(surface->format->BytesPerPixel, 4);
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
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
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
    }
}

TEST_F(GameLifetime, RepeatedFullGameStartupAndShutdownReleaseSubsystemsAndSubscriptions)
{
    const auto subscriptions = GameTestAccess::SubscriptionCount(game);
    game.Clean();
    for (int cycle = 0; cycle < 5; ++cycle)
    {
        SCOPED_TRACE(cycle);
        game.InitWindow("Repeated lifetime test", 800, 600);
        EXPECT_EQ(game.Registry().GetAmountOfAvailableEntities(), initialEntities);
        EXPECT_EQ(game.Physics().GetBodyCount(), initialBodies);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), subscriptions);
        auto particles = std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 4);
        particles->Initialise(game.Context());
        game.AddObject(std::move(particles));
        auto projectile = std::make_unique<Projectile>(100, 100, 0, 0);
        projectile->Initialise(game.Context());
        game.AddObject(std::move(projectile));
        game.QueueAdd(std::make_unique<ParticleSystem>("blood.png", 1, 0, 0, 2));
        game.Clean();
        EXPECT_FALSE(game.HasResources());
        EXPECT_TRUE(game.Objects().empty());
        EXPECT_TRUE(game.PendingAddCount() == 0);
        EXPECT_TRUE(game.PendingRemovalCount() == 0);
        EXPECT_EQ(GameTestAccess::SubscriptionCount(game), 0);
        EXPECT_FALSE(GameTestAccess::TakePendingException(game));
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
    Map map;
    map.Initialise(game.ContextWith(world));

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
    const auto subscription = game.Context().contacts.AddEvent(map.GetId(), CollisionType::END,
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
    EXPECT_NO_THROW(game.Context().contacts.RethrowPendingException());
    game.Context().contacts.RemoveEvent(subscription);
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
    FocusPoint focus;
    focus.Initialise(game.ContextWith(world));
    focus.ChangeTarget(target);
    camera->ChangeTarget(target);
    weapon->SetParent(target);
    WeaponTestAccess::SetCharge(*weapon);
    const auto pending = game.PendingAddCount();
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
    EXPECT_EQ(game.PendingAddCount(), pending);
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
