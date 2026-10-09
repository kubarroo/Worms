#include "Core/Camera/Camera.h"
#include "Core/GameObject.h"
#include "Game/Systems.h"
#include <gtest/gtest.h>
#include <memory>

TEST(HandleSafety, EntityZeroIsValidAndCleanupCanBeRepeated)
{
    auto world = std::make_unique<World>(nullptr);
    world->RegisterComponent<Position>();
    GameObject object;
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());
    EXPECT_THROW(object.Initialise(nullptr, nullptr), std::invalid_argument);

    object.Initialise(nullptr, world.get());
    EXPECT_TRUE(object.HasEntity());
    EXPECT_EQ(object.GetId(), 0);
    world->AddComponent<Position>(object.GetId(), {1, 2});
    EXPECT_THROW(object.Initialise(nullptr, world.get()), std::logic_error);

    object.CleanUp();
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());
}

TEST(HandleSafety, FocusPointCanHaveNoTargetOrTargetEntityZero)
{
    auto world = std::make_unique<World>(nullptr);
    world->RegisterComponent<Position>();
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {3, 4});
    FocusPoint focus(nullptr, world.get());
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(target);
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->get().x, 3);
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
    auto world = std::make_unique<World>(nullptr);
    world->RegisterComponent<Position>();
    const auto other = world->CreateEntity();
    world->AddComponent<Position>(other, {9, 9});
    Camera camera;
    EXPECT_THROW(camera.X(), std::logic_error);
    EXPECT_NO_THROW(camera.Update());
    camera.Initialise(nullptr, world.get());
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
    world->RegisterSystem<TargetSystem>();
    const auto entity = world->CreateEntity();
    world->AddComponent<Position>(entity, {5, 6});
    world->AddComponent<RigidBody>(entity);
    world->AddComponent<Follow>(entity);
    EXPECT_NO_THROW(world->Update());
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 5);

    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    world->GetComponent<Follow>(entity).id = target;
    world->Update();
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 1);
    world->DestroyEntity(target);
    EXPECT_NO_THROW(world->Update());
    EXPECT_FALSE(world->GetComponent<Follow>(entity).id);
}
