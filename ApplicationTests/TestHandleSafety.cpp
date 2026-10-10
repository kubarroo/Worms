#include "Core/Renderer2D.h"
#include "Core/Audio/Audio.h"
#include "Core/Input.h"
#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Core/ResourceManager.h"
#include "Core/Physics/ColliderFactory.h"
#include "Game/Systems.h"
#include <box2d/b2_world.h>
#include <gtest/gtest.h>
#include <memory>
#include <type_traits>

static_assert(std::is_default_constructible_v<GameObject>);
static_assert(!std::is_copy_constructible_v<GameObject>);
static_assert(!std::is_copy_assignable_v<GameObject>);
static_assert(!std::is_move_constructible_v<GameObject>);
static_assert(!std::is_move_assignable_v<GameObject>);
static_assert(!std::is_reference_v<decltype(std::declval<Camera&>().X())>);
static_assert(!std::is_reference_v<decltype(std::declval<Camera&>().Y())>);

namespace
{
class HeadlessCommands : public ObjectCommands
{
public:
    void QueueAdd(std::unique_ptr<GameObject>) override
    {
        throw std::logic_error("Headless test has no object queue");
    }
    void RequestDestroy(GameObject&) override
    {
        throw std::logic_error("Headless test has no object queue");
    }
};

class ScopedSdlTimer
{
public:
    ScopedSdlTimer() : result(SDL_Init(SDL_INIT_TIMER)) {}
    ~ScopedSdlTimer() { SDL_Quit(); }
    int Result() const { return result; }
    ScopedSdlTimer(const ScopedSdlTimer&) = delete;
    ScopedSdlTimer& operator=(const ScopedSdlTimer&) = delete;
private:
    int result;
};
}

TEST(HandleSafety, EntityZeroIsValidAndCleanupCanBeRepeated)
{
    auto world = std::make_unique<World>(nullptr);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts;
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    world->RegisterComponent<Position>();
    GameObject object;
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());

    object.Initialise(context);
    EXPECT_TRUE(object.HasEntity());
    EXPECT_EQ(object.GetId(), 0);
    world->AddComponent<Position>(object.GetId(), {1, 2});
    EXPECT_THROW(object.Initialise(context), std::logic_error);

    object.CleanUp();
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());
}

TEST(HandleSafety, FocusPointCanHaveNoTargetOrTargetEntityZero)
{
    auto world = std::make_unique<World>(nullptr);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts;
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    world->RegisterComponent<Position>();
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {3, 4});
    FocusPoint focus;
    focus.Initialise(context);
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(target);
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.ClearTarget();
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(target);
    world->DestroyEntity(target);
    EXPECT_FALSE(focus.GetPos());
    focus.CleanUp();
    EXPECT_FALSE(focus.GetPos());
}

TEST(HandleSafety, CameraReadsItsPositionAfterComponentCompaction)
{
    ScopedSdlTimer timer;
    ASSERT_EQ(timer.Result(), 0) << SDL_GetError();
    auto world = std::make_unique<World>(nullptr);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts;
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    world->RegisterComponent<Position>();
    const auto other = world->CreateEntity();
    world->AddComponent<Position>(other, {9, 9});
    Camera camera;
    EXPECT_THROW(camera.X(), std::logic_error);
    EXPECT_NO_THROW(camera.Update());
    camera.Initialise(context);
    world->DestroyEntity(other);
    EXPECT_FLOAT_EQ(camera.X(), 2);
    EXPECT_FLOAT_EQ(camera.Y(), -1);
    camera.ChangeX(1);
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(camera.GetId()).x, 3);
    EXPECT_NO_THROW(camera.Update());
    camera.CleanUp();
    EXPECT_NO_THROW(camera.CleanUp());
    EXPECT_NO_THROW(camera.Update());
    EXPECT_THROW(camera.Y(), std::logic_error);
}

TEST(HandleSafety, SystemsAcceptEmptyBodyAndMissingFollowTarget)
{
    auto world = std::make_unique<World>(nullptr);
    world->RegisterComponent<Position>();
    world->RegisterComponent<RigidBody>();
    world->RegisterComponent<Rotation>();
    world->RegisterComponent<Follow>();
    world->RegisterSystem<PhysicsSynchronizer>();
    world->RegisterSystem<TargetSystem>(*world);
    const auto entity = world->CreateEntity();
    world->AddComponent<Position>(entity, {5, 6});
    world->AddComponent<RigidBody>(entity);
    world->AddComponent<Follow>(entity);
    EXPECT_NO_THROW(world->Update());
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 5);

    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    world->GetComponent<Follow>(entity).id = world->GetHandle(target);
    world->Update();
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 1);
    world->DestroyEntity(target);
    EXPECT_NO_THROW(world->Update());
    EXPECT_FALSE(world->GetComponent<Follow>(entity).id);
}

TEST(HandleSafety, HandlesRejectReusedSlotsAndForeignWorlds)
{
    auto world = std::make_unique<World>(nullptr);
    auto otherWorld = std::make_unique<World>(nullptr);
    world->RegisterComponent<Position>();
    const auto target = world->CreateEntity();
    ASSERT_EQ(target, 0);
    world->AddComponent<Position>(target, {1, 2});
    const auto handle = world->GetHandle(target);
    ASSERT_TRUE(handle);
    EXPECT_TRUE(world->IsAlive(*handle));
    otherWorld->CreateEntity();
    EXPECT_FALSE(otherWorld->IsAlive(*handle));
    EXPECT_FALSE(world->IsAlive(EntityHandle{}));
    EXPECT_FALSE(world->GetHandle(static_cast<EntityId>(MAX_ENTITIES)));

    while (world->GetAmountOfAvailableEntities()) world->CreateEntity();
    EXPECT_THROW(world->CreateEntity(), std::runtime_error);
    world->DestroyEntity(target);
    world->DestroyEntity(target);
    EXPECT_EQ(world->GetAmountOfAvailableEntities(), 1);
    EXPECT_FALSE(world->IsAlive(*handle));
    const auto replacement = world->CreateEntity();
    ASSERT_EQ(replacement, target);
    EXPECT_FALSE(world->IsAlive(*handle));
    EXPECT_TRUE(world->IsAlive(*world->GetHandle(replacement)));
    EXPECT_FALSE(world->TryGetComponent<Position>(replacement));
    // A stale signature would make this second destruction erase a missing component.
    EXPECT_NO_THROW(world->DestroyEntity(replacement));
    EXPECT_EQ(world->GetAmountOfAvailableEntities(), 1);
}

TEST(HandleSafety, ObserversClearMissingPositionAndDoNotResumeAfterItIsAddedBack)
{
    auto world = std::make_unique<World>(nullptr);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts;
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    world->RegisterComponent<Position>();
    world->RegisterComponent<Follow>();
    world->RegisterSystem<TargetSystem>(*world);
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    const auto follower = world->CreateEntity();
    world->AddComponent<Position>(follower, {5, 6});
    world->AddComponent<Follow>(follower, {world->GetHandle(target), 0, 0});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(target);
    const auto snapshot = focus.GetPos();
    ASSERT_TRUE(snapshot);
    world->RemoveComponent<Position>(target);
    EXPECT_TRUE(world->IsAlive(target));
    EXPECT_FALSE(focus.GetPos());
    world->Update();
    EXPECT_FALSE(world->GetComponent<Follow>(follower).id);
    world->AddComponent<Position>(target, {9, 10});
    EXPECT_FALSE(focus.GetPos());
    world->Update();
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(follower).x, 5);
    EXPECT_FLOAT_EQ(snapshot->x, 1);
    focus.CleanUp();
}

TEST(HandleSafety, FocusPointResolvesPositionAfterAnotherComponentIsCompacted)
{
    auto world = std::make_unique<World>(nullptr);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts;
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    world->RegisterComponent<Position>();
    const auto other = world->CreateEntity();
    world->AddComponent<Position>(other, {9, 10});
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(target);
    world->DestroyEntity(other);
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 1);
    world->GetComponent<Position>(target).x = 3;
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.CleanUp();
}
