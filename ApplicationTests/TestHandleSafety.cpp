#include "Core/Renderer2D.h"
#include "Core/Audio/Audio.h"
#include "Core/Input.h"
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/ResourceManager.h"
#include "Core/SDLHandles.h"
#include "Core/Physics/ColliderFactory.h"
#include "Game/Systems.h"
#include "Game/RenderOrder.h"
#include "TestRegistry.h"
#include <box2d/b2_world.h>
#include <gtest/gtest.h>
#include <memory>
#include <numbers>
#include <type_traits>

static_assert(std::is_default_constructible_v<GameObject>);
static_assert(!std::is_copy_constructible_v<GameObject>);
static_assert(!std::is_copy_assignable_v<GameObject>);
static_assert(!std::is_move_constructible_v<GameObject>);
static_assert(!std::is_move_assignable_v<GameObject>);
static_assert(!std::is_reference_v<decltype(std::declval<Camera&>().X())>);
static_assert(!std::is_reference_v<decltype(std::declval<Camera&>().Y())>);
static_assert(!std::is_invocable_v<decltype(&Camera::ChangeTarget), Camera&, EntityId>);
static_assert(!std::is_invocable_v<decltype(&FocusPoint::ChangeTarget), FocusPoint&, EntityId>);

namespace
{
class HeadlessCommands : public ObjectCommands
{
public:
    ~HeadlessCommands() override
    {
        for (auto& object : retired) object->CleanUp();
    }
    void RetireObject(std::unique_ptr<GameObject> object) override
    {
        retired.push_back(std::move(object));
    }
    void QueueAdd(std::unique_ptr<GameObject>) override
    {
        throw std::logic_error("Headless test has no object queue");
    }
    void RequestDestroy(GameObject&) override
    {
        throw std::logic_error("Headless test has no object queue");
    }
private:
    std::vector<std::unique_ptr<GameObject>> retired;
};

class ScopedSdlRuntime
{
public:
    ScopedSdlRuntime() : result(SDL_Init(0)) {}
    ~ScopedSdlRuntime() { SDL_Quit(); }
    bool Result() const { return result; }
    ScopedSdlRuntime(const ScopedSdlRuntime&) = delete;
    ScopedSdlRuntime& operator=(const ScopedSdlRuntime&) = delete;
private:
    bool result;
};
}

TEST(HandleSafety, EntityZeroIsValidAndCleanupCanBeRepeated)
{
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    GameObject object;
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_THROW(object.GetHandle(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());

    object.Initialise(context);
    EXPECT_TRUE(object.HasEntity());
    EXPECT_TRUE(IsValid(world, object.GetHandle()));
    EXPECT_EQ(entt::to_integral(object.GetId()), 0u);
    world.emplace<Position>(object.GetId(), Position{1, 2});
    EXPECT_THROW(object.Initialise(context), std::logic_error);

    object.CleanUp();
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());

    object.Initialise(context);
    const auto stale = object.GetHandle();
    registry.destroy(stale.id);
    const auto replacement = registry.create();
    EXPECT_THROW(object.GetHandle(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());
    EXPECT_TRUE(registry.valid(replacement));
    EXPECT_NO_THROW(object.CleanUp());
    registry.destroy(replacement);
}

TEST(HandleSafety, FocusPointCanHaveNoTargetOrTargetEntityZero)
{
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto target = world.create();
    world.emplace<Position>(target, Position{3, 4});
    FocusPoint focus;
    focus.Initialise(context);
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(*MakeHandle(world, target));
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.ClearTarget();
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(*MakeHandle(world, target));
    world.destroy(target);
    EXPECT_FALSE(focus.GetPos());
    focus.CleanUp();
    EXPECT_FALSE(focus.GetPos());
}

TEST(HandleSafety, CameraReadsItsPositionAfterComponentCompaction)
{
    ScopedSdlRuntime runtime;
    ASSERT_TRUE(runtime.Result()) << SDL_GetError();
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto other = world.create();
    world.emplace<Position>(other, Position{9, 9});
    Camera camera;
    EXPECT_THROW(camera.X(), std::logic_error);
    EXPECT_NO_THROW(camera.Update());
    camera.Initialise(context);
    world.destroy(other);
    EXPECT_FLOAT_EQ(camera.X(), 2);
    EXPECT_FLOAT_EQ(camera.Y(), -1);
    camera.ChangeX(1);
    EXPECT_FLOAT_EQ(world.get<Position>(camera.GetId()).x, 3);
    EXPECT_NO_THROW(camera.Update());
    registry.remove<Position>(camera.GetId());
    EXPECT_THROW(camera.X(), std::logic_error);
    registry.emplace<Position>(camera.GetId(), Position{3, -1});
    camera.CleanUp();
    EXPECT_NO_THROW(camera.CleanUp());
    EXPECT_NO_THROW(camera.Update());
    EXPECT_THROW(camera.Y(), std::logic_error);
}

TEST(HandleSafety, SystemsAcceptEmptyBodyAndMissingFollowTarget)
{
    entt::registry registry;
    auto& world = registry;
    const auto entity = world.create();
    world.emplace<Position>(entity, Position{5, 6});
    world.emplace<RigidBody>(entity);
    world.emplace<Follow>(entity);
    EXPECT_NO_THROW(PhysicsSynchronizer::Update(registry));
    EXPECT_NO_THROW(TargetSystem::Update(registry));
    EXPECT_FLOAT_EQ(world.get<Position>(entity).x, 5);

    const auto target = world.create();
    world.emplace<Position>(target, Position{1, 2});
    world.get<Follow>(entity).id = MakeHandle(world, target);
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(world.get<Position>(entity).x, 1);
    world.destroy(target);
    EXPECT_NO_THROW(TargetSystem::Update(registry));
    EXPECT_FALSE(world.get<Follow>(entity).id);
}

TEST(HandleSafety, HandlesRejectReusedSlotsAndForeignWorlds)
{
    entt::registry registry;
    auto& world = registry;
    entt::registry otherRegistry;
    auto& otherWorld = otherRegistry;
    const auto target = world.create();
    ASSERT_EQ(entt::to_integral(target), 0u);
    world.emplace<Position>(target, Position{1, 2});
    const auto handle = MakeHandle(world, target);
    ASSERT_TRUE(handle);
    EXPECT_TRUE(IsValid(world, *handle));
    const auto foreignEntity = otherWorld.create();
    EXPECT_TRUE(otherWorld.valid(foreignEntity));
    EXPECT_FALSE(IsValid(otherWorld, *handle));
    EXPECT_FALSE(IsValid(world, EntityHandle{}));
    EXPECT_FALSE(MakeHandle(world, entt::null));

    world.destroy(target);
    EXPECT_FALSE(world.valid(target));
    EXPECT_EQ(EntityCount(world), 0u);
    EXPECT_FALSE(IsValid(world, *handle));
    const auto replacement = world.create();
    ASSERT_EQ(entt::to_entity(replacement), entt::to_entity(target));
    EXPECT_NE(replacement, target);
    EXPECT_FALSE(IsValid(world, *handle));
    EXPECT_TRUE(IsValid(world, *MakeHandle(world, replacement)));
    EXPECT_FALSE(world.try_get<Position>(replacement));
    // Recycled entities start without components and reject the old identifier.
    EXPECT_NO_THROW(world.destroy(replacement));
    EXPECT_EQ(EntityCount(world), 0u);
}

TEST(HandleSafety, ObserversClearMissingPositionAndDoNotResumeAfterItIsAddedBack)
{
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto target = world.create();
    world.emplace<Position>(target, Position{1, 2});
    const auto follower = world.create();
    world.emplace<Position>(follower, Position{5, 6});
    world.emplace<Follow>(follower, Follow{MakeHandle(world, target), 0, 0});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(*MakeHandle(world, target));
    const auto snapshot = focus.GetPos();
    ASSERT_TRUE(snapshot);
    world.remove<Position>(target);
    EXPECT_TRUE(world.valid(target));
    EXPECT_FALSE(focus.GetPos());
    TargetSystem::Update(registry);
    EXPECT_FALSE(world.get<Follow>(follower).id);
    world.emplace<Position>(target, Position{9, 10});
    EXPECT_FALSE(focus.GetPos());
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(world.get<Position>(follower).x, 5);
    EXPECT_FLOAT_EQ(snapshot->x, 1);
    focus.CleanUp();
}

TEST(HandleSafety, FocusPointResolvesPositionAfterAnotherComponentIsCompacted)
{
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto other = world.create();
    world.emplace<Position>(other, Position{9, 10});
    const auto target = world.create();
    world.emplace<Position>(target, Position{1, 2});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(*MakeHandle(world, target));
    world.destroy(other);
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 1);
    world.get<Position>(target).x = 3;
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.CleanUp();
}

TEST(HandleSafety, RegistryComponentsCanBeReadRemovedAndRecreated)
{
    entt::registry registry;
    auto& world = registry;
    const auto entity = registry.create();
    registry.emplace<Position>(entity, 3.f, 4.f);
    EXPECT_EQ(EntityCount(world), 1u);
    world.get<Position>(entity).x = 5;
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 5);
    EXPECT_TRUE(world.all_of<Position>(entity));
    world.remove<Position>(entity);
    EXPECT_EQ(world.try_get<Position>(entity), nullptr);
    world.emplace<Position>(entity, Position{6, 7});
    EXPECT_FLOAT_EQ(world.get<Position>(entity).x, 6);
    world.destroy(entity);
    EXPECT_FALSE(registry.valid(entity));
    EXPECT_FALSE(world.try_get<Position>(entity));
    EXPECT_FALSE(MakeHandle(world, entity));
    EXPECT_EQ(EntityCount(world), 0u);
}

TEST(HandleSafety, RegistryGrowsBeyondTheFormerEntityLimit)
{
    entt::registry registry;
    auto& world = registry;
    for (std::size_t count = 0; count < 50001; ++count)
        static_cast<void>(world.create());
    EXPECT_EQ(EntityCount(world), 50001u);
    registry.clear();
    EXPECT_EQ(EntityCount(world), 0u);
}

TEST(HandleSafety, ViewsUpdateMatchingComponentsAndRejectForeignFollowHandles)
{
    entt::registry registry, foreignRegistry;
    auto& world = registry; auto& foreign = foreignRegistry;
    const auto particle = world.create();
    world.emplace<Position>(particle, Position{1, 2});
    world.emplace<Motion>(particle, Motion{3, 4});
    world.emplace<Scale>(particle, Scale{2});
    world.emplace<Sprite>(particle);
    world.emplace<Particle>(particle, Particle{nullptr, nullptr,
         [](float x, float y) { return std::pair{x * 2, y * 2}; },
         [](float size) { return size / 2; }});
    const auto stationary = world.create();
    world.emplace<Position>(stationary, Position{7, 8});
    Movement::Update(registry);
    ParticleUpdater::Update(registry);
    EXPECT_FLOAT_EQ(world.get<Position>(particle).x, 4);
    EXPECT_FLOAT_EQ(world.get<Motion>(particle).v_y, 8);
    EXPECT_FLOAT_EQ(world.get<Scale>(particle).size, 1);
    EXPECT_FLOAT_EQ(world.get<Position>(stationary).x, 7);
    const auto foreignTarget = foreign.create();
    foreign.emplace<Position>(foreignTarget, Position{90, 90});
    world.emplace<Follow>(stationary, Follow{MakeHandle(foreign, foreignTarget), 0, 0});
    TargetSystem::Update(registry);
    EXPECT_FALSE(world.get<Follow>(stationary).id);
    EXPECT_FLOAT_EQ(world.get<Position>(stationary).x, 7);
}

TEST(HandleSafety, MovementTracksAddedAndRemovedComponents)
{
    entt::registry registry;
    const auto entity = registry.create();
    registry.emplace<Position>(entity, Position{1, 2});
    const auto motionOnly = registry.create();
    registry.emplace<Motion>(motionOnly, Motion{9, 10});
    Movement::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 1);
    registry.emplace<Motion>(entity, Motion{3, -4});
    Movement::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 4);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).y, -2);
    EXPECT_FLOAT_EQ(registry.get<Motion>(entity).v_x, 3);
    registry.remove<Motion>(entity);
    Movement::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 4);
    registry.emplace<Motion>(entity, Motion{2, 1});
    registry.remove<Position>(entity);
    Movement::Update(registry);
    registry.emplace<Position>(entity, Position{10, 20});
    Movement::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 12);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).y, 21);
    EXPECT_FALSE(registry.all_of<Position>(motionOnly));
}

TEST(HandleSafety, PhysicsSynchronizerCopiesTransformsAndTracksViewMembership)
{
    entt::registry registry;
    b2World physics({0, 0});
    b2BodyDef definition;
    definition.position.Set(3, 4);
    definition.angle = static_cast<float>(std::numbers::pi / 6);
    auto* body = physics.CreateBody(&definition);
    const auto entity = registry.create();
    registry.emplace<Position>(entity, Position{-1, -2});
    registry.emplace<Rotation>(entity, Rotation{-45});
    const auto bodyOnly = registry.create();
    registry.emplace<RigidBody>(bodyOnly, RigidBody{body});
    const auto emptyBody = registry.create();
    registry.emplace<Position>(emptyBody, Position{7, 8});
    registry.emplace<Rotation>(emptyBody, Rotation{90});
    registry.emplace<RigidBody>(emptyBody);
    PhysicsSynchronizer::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, -1);
    registry.emplace<RigidBody>(entity, RigidBody{body});
    PhysicsSynchronizer::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 3);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).y, 4);
    EXPECT_NEAR(registry.get<Rotation>(entity).degree, 30, 0.0001f);
    EXPECT_FLOAT_EQ(registry.get<Position>(emptyBody).x, 7);
    EXPECT_FLOAT_EQ(registry.get<Rotation>(emptyBody).degree, 90);
    EXPECT_FALSE(registry.all_of<Position>(bodyOnly));

    registry.remove<Rotation>(entity);
    body->SetTransform({9, 10}, static_cast<float>(-std::numbers::pi / 2));
    PhysicsSynchronizer::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 9);
    EXPECT_FALSE(registry.all_of<Rotation>(entity));
    registry.emplace<Rotation>(entity, Rotation{0});
    registry.remove<RigidBody>(entity);
    body->SetTransform({11, 12}, static_cast<float>(std::numbers::pi));
    PhysicsSynchronizer::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 9);
    EXPECT_FLOAT_EQ(registry.get<Rotation>(entity).degree, 0);
    registry.emplace<RigidBody>(entity, RigidBody{body});
    PhysicsSynchronizer::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 11);
    EXPECT_NEAR(registry.get<Rotation>(entity).degree, 180, 0.0001f);
}

TEST(HandleSafety, ParticleUpdaterRequiresAllComponentsAndRejoinsAfterInsertion)
{
    const auto exercise = []<typename Missing>()
    {
        entt::registry registry;
        int velocityCalls = 0, sizeCalls = 0;
        const auto entity = registry.create();
        registry.emplace<Position>(entity, Position{1, 2});
        registry.emplace<Motion>(entity, Motion{2, 3});
        registry.emplace<Scale>(entity, Scale{8});
        registry.emplace<Sprite>(entity);
        registry.emplace<Particle>(entity, Particle{nullptr, nullptr,
            [&](float x, float y) { ++velocityCalls; return std::pair{x * 2, y * 2}; },
            [&](float size) { ++sizeCalls; return size / 2; }});
        const auto removed = registry.get<Missing>(entity);
        registry.remove<Missing>(entity);
        ParticleUpdater::Update(registry);
        EXPECT_EQ(velocityCalls, 0);
        EXPECT_EQ(sizeCalls, 0);
        registry.emplace<Missing>(entity, removed);
        ParticleUpdater::Update(registry);
        EXPECT_EQ(velocityCalls, 1);
        EXPECT_EQ(sizeCalls, 1);
        EXPECT_FLOAT_EQ(registry.get<Motion>(entity).v_x, 4);
        EXPECT_FLOAT_EQ(registry.get<Motion>(entity).v_y, 6);
        EXPECT_FLOAT_EQ(registry.get<Scale>(entity).size, 4);
        EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 1);
        EXPECT_EQ(registry.get<Sprite>(entity).texture, nullptr);
    };
    exercise.operator()<Particle>();
    exercise.operator()<Position>();
    exercise.operator()<Motion>();
    exercise.operator()<Scale>();
    exercise.operator()<Sprite>();
}

TEST(HandleSafety, TargetSystemTracksFollowerMembershipAndPreservesOffsets)
{
    entt::registry registry;
    const auto target = registry.create();
    registry.emplace<Position>(target, Position{10, 20});
    const auto follower = registry.create();
    registry.emplace<Follow>(follower, Follow{EntityHandle{target, &registry}, 2, -3});
    TargetSystem::Update(registry);
    EXPECT_TRUE(registry.get<Follow>(follower).id);
    registry.emplace<Position>(follower, Position{0, 0});
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(follower).x, 12);
    EXPECT_FLOAT_EQ(registry.get<Position>(follower).y, 17);
    const auto follow = registry.get<Follow>(follower);
    registry.remove<Follow>(follower);
    registry.get<Position>(target) = {30, 40};
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(follower).x, 12);
    registry.emplace<Follow>(follower, follow);
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(registry.get<Position>(follower).x, 32);
    EXPECT_FLOAT_EQ(registry.get<Position>(follower).y, 37);
}

TEST(HandleSafety, ParticleCallbacksCanCompactPoolsWithoutUsingOldReferences)
{
    entt::registry registry;
    const auto makeParticle = [&]
    {
        const auto id = registry.create();
        registry.emplace<Particle>(id);
        registry.emplace<Position>(id);
        registry.emplace<Scale>(id, Scale{4});
        registry.emplace<Sprite>(id);
        registry.emplace<Motion>(id, Motion{2, 3});
        return id;
    };
    const auto victim = makeParticle();
    const auto current = makeParticle();
    registry.get<Particle>(current).vel_characteristic = [&](float x, float y)
    {
        registry.destroy(victim);
        return std::pair{x + 5, y + 6};
    };
    registry.get<Particle>(current).size_characteristic = [](float size) { return size / 2; };
    EXPECT_NO_THROW(ParticleUpdater::Update(registry));
    EXPECT_FALSE(registry.valid(victim));
    EXPECT_FLOAT_EQ(registry.get<Motion>(current).v_x, 7);
    EXPECT_FLOAT_EQ(registry.get<Motion>(current).v_y, 9);
    EXPECT_FLOAT_EQ(registry.get<Scale>(current).size, 2);
}

TEST(HandleSafety, ParticleCallbacksPreserveMutableStateAcrossUpdatesAndCompaction)
{
    entt::registry registry;
    const auto victim = registry.create();
    const auto current = registry.create();
    registry.emplace<Particle>(victim);
    registry.emplace<Motion>(victim);
    registry.emplace<Scale>(victim);
    registry.emplace<Particle>(current);
    registry.emplace<Position>(current);
    registry.emplace<Motion>(current);
    registry.emplace<Scale>(current);
    registry.emplace<Sprite>(current);
    registry.get<Particle>(current).vel_characteristic =
        [&, calls = 0](float, float) mutable
        {
            if (++calls == 1) registry.destroy(victim);
            return std::pair{static_cast<float>(calls), static_cast<float>(calls * 2)};
        };
    registry.get<Particle>(current).size_characteristic =
        [calls = 0](float) mutable { return static_cast<float>(++calls); };
    for (int frame = 1; frame <= 3; ++frame)
    {
        EXPECT_NO_THROW(ParticleUpdater::Update(registry));
        EXPECT_FLOAT_EQ(registry.get<Motion>(current).v_x, static_cast<float>(frame));
        EXPECT_FLOAT_EQ(registry.get<Motion>(current).v_y, static_cast<float>(frame * 2));
        EXPECT_FLOAT_EQ(registry.get<Scale>(current).size, static_cast<float>(frame));
    }
}

TEST(HandleSafety, ParticleCallbackCannotWriteToRecycledEntityOrUseRemovedComponents)
{
    entt::registry registry;
    const auto makeParticle = [&]
    {
        const auto id = registry.create();
        registry.emplace<Particle>(id);
        registry.emplace<Position>(id);
        registry.emplace<Scale>(id, Scale{4});
        registry.emplace<Sprite>(id);
        registry.emplace<Motion>(id, Motion{2, 3});
        return id;
    };
    const auto current = makeParticle();
    EntityId replacement = entt::null;
    registry.get<Particle>(current).vel_characteristic = [&](float, float)
    {
        registry.destroy(current);
        replacement = makeParticle();
        return std::pair{100.f, 200.f};
    };
    EXPECT_NO_THROW(ParticleUpdater::Update(registry));
    ASSERT_TRUE(registry.valid(replacement));
    EXPECT_NE(current, replacement);
    EXPECT_EQ(entt::to_entity(current), entt::to_entity(replacement));
    EXPECT_FLOAT_EQ(registry.get<Motion>(replacement).v_x, 2);
    registry.get<Particle>(replacement).size_characteristic = [&](float)
    {
        registry.remove<Scale>(replacement);
        return 100.f;
    };
    EXPECT_NO_THROW(ParticleUpdater::Update(registry));
    EXPECT_FALSE(registry.all_of<Scale>(replacement));
}

TEST(HandleSafety, SpriteRendererUsesExplicitOrderAcrossCompactionSlotReuseAndLayerChanges)
{
    ScopedSdlRuntime runtime;
    ASSERT_TRUE(runtime.Result());
    Sdl::SurfacePtr canvas(SDL_CreateSurface(800, 600, SDL_PIXELFORMAT_RGBA32));
    ASSERT_TRUE(canvas);
    Sdl::RendererPtr native(SDL_CreateSoftwareRenderer(canvas.get()));
    ASSERT_TRUE(native);
    Renderer2D rendering(native.get());
    entt::registry registry;
    auto& world = registry;
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Audio audio;
    Input input;
    const SceneContext context{native.get(), world, physics, commands, colliders, contacts,
                               resources, rendering, audio, input};
    Camera camera;
    camera.Initialise(context);
    Sdl::SurfacePtr source(SDL_CreateSurface(2, 2, SDL_PIXELFORMAT_RGBA32));
    ASSERT_TRUE(source);
    ASSERT_TRUE(SDL_FillSurfaceRect(source.get(), nullptr,
                                   SDL_MapSurfaceRGBA(source.get(), 255, 0, 0, 255)));
    Sdl::TexturePtr red(SDL_CreateTextureFromSurface(native.get(), source.get()));
    ASSERT_TRUE(red);
    ASSERT_TRUE(SDL_FillSurfaceRect(source.get(), nullptr,
                                   SDL_MapSurfaceRGBA(source.get(), 0, 0, 255, 255)));
    Sdl::TexturePtr blue(SDL_CreateTextureFromSurface(native.get(), source.get()));
    ASSERT_TRUE(blue);
    const auto removed = world.create();
    world.emplace<Sprite>(removed);
    world.emplace<RenderOrder>(removed, NextRenderOrder(registry, RenderLayer::Actors));
    const auto back = world.create();
    const auto front = world.create();
    for (const auto entity : {back, front})
        world.emplace<Position>(entity, Position{2, -1});
    world.emplace<Sprite>(back, Sprite{red.get()});
    world.emplace<Sprite>(front, Sprite{blue.get()});
    world.emplace<RenderOrder>(back, NextRenderOrder(registry, RenderLayer::Actors));
    world.emplace<RenderOrder>(front, NextRenderOrder(registry, RenderLayer::Actors));
    world.destroy(removed);
    const auto drawPixel = [&]
    {
        rendering.Clear({0, 0, 0, 255});
        SpriteRenderer::Render(registry, rendering, camera);
        rendering.Present();
        const auto* row = reinterpret_cast<const Uint32*>(
            static_cast<const Uint8*>(canvas->pixels) + 300 * canvas->pitch);
        Uint8 r, g, b, a;
        SDL_GetRGBA(row[400], SDL_GetPixelFormatDetails(canvas->format), nullptr, &r, &g, &b, &a);
        return RenderColor{r, g, b, a};
    };
    EXPECT_EQ(drawPixel().b, 255);

    // A new foreground sprite reuses a lower slot but must still cover the older blue sprite.
    const auto reused = world.create();
    ASSERT_EQ(entt::to_entity(reused), entt::to_entity(removed));
    EXPECT_NE(reused, removed);
    world.emplace<Position>(reused, Position{2, -1});
    world.emplace<Sprite>(reused, Sprite{red.get()});
    world.emplace<RenderOrder>(reused, NextRenderOrder(registry, RenderLayer::Actors));
    EXPECT_EQ(drawPixel().r, 255);

    // Layer takes precedence over creation order, including changes made after a draw.
    world.get<RenderOrder>(reused).layer = RenderLayer::Terrain;
    EXPECT_EQ(drawPixel().b, 255);
    world.get<RenderOrder>(back).layer = RenderLayer::Effects;
    EXPECT_EQ(drawPixel().r, 255);
    world.remove<RenderOrder>(back);
    EXPECT_EQ(drawPixel().b, 255);
    world.emplace<RenderOrder>(back, NextRenderOrder(registry, RenderLayer::Effects));
    EXPECT_EQ(drawPixel().r, 255);
    world.remove<Sprite>(back);
    EXPECT_EQ(drawPixel().b, 255);
    world.emplace<Sprite>(back, Sprite{red.get()});
    world.remove<Position>(back);
    EXPECT_EQ(drawPixel().b, 255);
    world.emplace<Position>(back, Position{2, -1});
    EXPECT_EQ(drawPixel().r, 255);
    camera.CleanUp();
}
