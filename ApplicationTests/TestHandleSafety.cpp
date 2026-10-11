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
static_assert(!std::is_invocable_v<decltype(&Camera::ChangeTarget), Camera&, EntityId>);
static_assert(!std::is_invocable_v<decltype(&FocusPoint::ChangeTarget), FocusPoint&, EntityId>);

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
    auto world = std::make_unique<World>(registry);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    GameObject object;
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_THROW(object.GetHandle(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());

    object.Initialise(context);
    EXPECT_TRUE(object.HasEntity());
    EXPECT_TRUE(world->IsAlive(object.GetHandle()));
    EXPECT_EQ(entt::to_integral(object.GetId()), 0u);
    world->AddComponent<Position>(object.GetId(), {1, 2});
    EXPECT_THROW(object.Initialise(context), std::logic_error);

    object.CleanUp();
    EXPECT_FALSE(object.HasEntity());
    EXPECT_THROW(object.GetId(), std::logic_error);
    EXPECT_NO_THROW(object.CleanUp());
}

TEST(HandleSafety, FocusPointCanHaveNoTargetOrTargetEntityZero)
{
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {3, 4});
    FocusPoint focus;
    focus.Initialise(context);
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(*world->GetHandle(target));
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.ClearTarget();
    EXPECT_FALSE(focus.GetPos());
    focus.ChangeTarget(*world->GetHandle(target));
    world->DestroyEntity(target);
    EXPECT_FALSE(focus.GetPos());
    focus.CleanUp();
    EXPECT_FALSE(focus.GetPos());
}

TEST(HandleSafety, CameraReadsItsPositionAfterComponentCompaction)
{
    ScopedSdlRuntime runtime;
    ASSERT_TRUE(runtime.Result()) << SDL_GetError();
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
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
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    const auto entity = world->CreateEntity();
    world->AddComponent<Position>(entity, {5, 6});
    world->AddComponent<RigidBody>(entity);
    world->AddComponent<Follow>(entity);
    EXPECT_NO_THROW(PhysicsSynchronizer::Update(registry));
    EXPECT_NO_THROW(TargetSystem::Update(registry));
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 5);

    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    world->GetComponent<Follow>(entity).id = world->GetHandle(target);
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(entity).x, 1);
    world->DestroyEntity(target);
    EXPECT_NO_THROW(TargetSystem::Update(registry));
    EXPECT_FALSE(world->GetComponent<Follow>(entity).id);
}

TEST(HandleSafety, HandlesRejectReusedSlotsAndForeignWorlds)
{
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    entt::registry otherRegistry;
    auto otherWorld = std::make_unique<World>(otherRegistry);
    const auto target = world->CreateEntity();
    ASSERT_EQ(entt::to_integral(target), 0u);
    world->AddComponent<Position>(target, {1, 2});
    const auto handle = world->GetHandle(target);
    ASSERT_TRUE(handle);
    EXPECT_TRUE(world->IsAlive(*handle));
    otherWorld->CreateEntity();
    EXPECT_FALSE(otherWorld->IsAlive(*handle));
    EXPECT_FALSE(world->IsAlive(EntityHandle{}));
    EXPECT_FALSE(world->GetHandle(entt::null));

    world->DestroyEntity(target);
    world->DestroyEntity(target);
    EXPECT_EQ(world->GetEntityCount(), 0u);
    EXPECT_FALSE(world->IsAlive(*handle));
    const auto replacement = world->CreateEntity();
    ASSERT_EQ(entt::to_entity(replacement), entt::to_entity(target));
    EXPECT_NE(replacement, target);
    EXPECT_FALSE(world->IsAlive(*handle));
    EXPECT_TRUE(world->IsAlive(*world->GetHandle(replacement)));
    EXPECT_FALSE(world->TryGetComponent<Position>(replacement));
    // Recycled entities start without components and reject the old identifier.
    EXPECT_NO_THROW(world->DestroyEntity(replacement));
    EXPECT_EQ(world->GetEntityCount(), 0u);
}

TEST(HandleSafety, ObserversClearMissingPositionAndDoNotResumeAfterItIsAddedBack)
{
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    const auto follower = world->CreateEntity();
    world->AddComponent<Position>(follower, {5, 6});
    world->AddComponent<Follow>(follower, {world->GetHandle(target), 0, 0});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(*world->GetHandle(target));
    const auto snapshot = focus.GetPos();
    ASSERT_TRUE(snapshot);
    world->RemoveComponent<Position>(target);
    EXPECT_TRUE(world->IsAlive(target));
    EXPECT_FALSE(focus.GetPos());
    TargetSystem::Update(registry);
    EXPECT_FALSE(world->GetComponent<Follow>(follower).id);
    world->AddComponent<Position>(target, {9, 10});
    EXPECT_FALSE(focus.GetPos());
    TargetSystem::Update(registry);
    EXPECT_FLOAT_EQ(world->GetComponent<Position>(follower).x, 5);
    EXPECT_FLOAT_EQ(snapshot->x, 1);
    focus.CleanUp();
}

TEST(HandleSafety, FocusPointResolvesPositionAfterAnotherComponentIsCompacted)
{
    entt::registry registry;
    auto world = std::make_unique<World>(registry);
    b2World physics({0, 0});
    HeadlessCommands commands;
    ContactManager contacts(registry);
    ColliderFactory colliders(physics, contacts);
    ResourceManager resources(nullptr, ".");
    Renderer2D rendering(nullptr);
    Audio audio;
    Input input;
    const SceneContext context{nullptr, *world, physics, commands, colliders, contacts, resources,
                               rendering, audio, input};
    const auto other = world->CreateEntity();
    world->AddComponent<Position>(other, {9, 10});
    const auto target = world->CreateEntity();
    world->AddComponent<Position>(target, {1, 2});
    FocusPoint focus;
    focus.Initialise(context);
    focus.ChangeTarget(*world->GetHandle(target));
    world->DestroyEntity(other);
    ASSERT_TRUE(focus.GetPos());
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 1);
    world->GetComponent<Position>(target).x = 3;
    EXPECT_FLOAT_EQ(focus.GetPos()->x, 3);
    focus.CleanUp();
}

TEST(HandleSafety, AdapterAndDirectRegistryAccessShareTheSameStorage)
{
    entt::registry registry;
    World world(registry);
    const auto entity = registry.create();
    registry.emplace<Position>(entity, 3.f, 4.f);
    EXPECT_EQ(world.GetEntityCount(), 1u);
    world.GetComponent<Position>(entity).x = 5;
    EXPECT_FLOAT_EQ(registry.get<Position>(entity).x, 5);
    EXPECT_THROW(world.AddComponent<Position>(entity), std::logic_error);
    world.DestroyEntity(entity);
    EXPECT_FALSE(registry.valid(entity));
    EXPECT_FALSE(world.TryGetComponent<Position>(entity));
    EXPECT_THROW(world.GetComponent<Position>(entity), std::logic_error);
    EXPECT_THROW(world.AddComponent<Position>(entity), std::logic_error);
    EXPECT_EQ(world.GetEntityCount(), 0u);
}

TEST(HandleSafety, RegistryGrowsBeyondTheFormerEntityLimit)
{
    entt::registry registry;
    World world(registry);
    for (std::size_t count = 0; count < 50001; ++count)
        world.CreateEntity();
    EXPECT_EQ(world.GetEntityCount(), 50001u);
    registry.clear();
    EXPECT_EQ(world.GetEntityCount(), 0u);
}

TEST(HandleSafety, ViewsUpdateMatchingComponentsAndRejectForeignFollowHandles)
{
    entt::registry registry, foreignRegistry;
    World world(registry), foreign(foreignRegistry);
    const auto particle = world.CreateEntity();
    world.AddComponent<Position>(particle, {1, 2});
    world.AddComponent<Motion>(particle, {3, 4});
    world.AddComponent<Scale>(particle, {2});
    world.AddComponent<Sprite>(particle);
    world.AddComponent<Particle>(particle,
        {nullptr, nullptr,
         [](float x, float y) { return std::pair{x * 2, y * 2}; },
         [](float size) { return size / 2; }});
    const auto stationary = world.CreateEntity();
    world.AddComponent<Position>(stationary, {7, 8});
    Movement::Update(registry);
    ParticleUpdater::Update(registry);
    EXPECT_FLOAT_EQ(world.GetComponent<Position>(particle).x, 4);
    EXPECT_FLOAT_EQ(world.GetComponent<Motion>(particle).v_y, 8);
    EXPECT_FLOAT_EQ(world.GetComponent<Scale>(particle).size, 1);
    EXPECT_FLOAT_EQ(world.GetComponent<Position>(stationary).x, 7);
    const auto foreignTarget = foreign.CreateEntity();
    foreign.AddComponent<Position>(foreignTarget, {90, 90});
    world.AddComponent<Follow>(stationary, {foreign.GetHandle(foreignTarget), 0, 0});
    TargetSystem::Update(registry);
    EXPECT_FALSE(world.GetComponent<Follow>(stationary).id);
    EXPECT_FLOAT_EQ(world.GetComponent<Position>(stationary).x, 7);
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
    World world(registry);
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
    const auto removed = world.CreateEntity();
    world.AddComponent<Sprite>(removed);
    world.AddComponent<RenderOrder>(removed, NextRenderOrder(registry, RenderLayer::Actors));
    const auto back = world.CreateEntity();
    const auto front = world.CreateEntity();
    for (const auto entity : {back, front})
        world.AddComponent<Position>(entity, {2, -1});
    world.AddComponent<Sprite>(back, {red.get()});
    world.AddComponent<Sprite>(front, {blue.get()});
    world.AddComponent<RenderOrder>(back, NextRenderOrder(registry, RenderLayer::Actors));
    world.AddComponent<RenderOrder>(front, NextRenderOrder(registry, RenderLayer::Actors));
    world.DestroyEntity(removed);
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
    const auto reused = world.CreateEntity();
    ASSERT_EQ(entt::to_entity(reused), entt::to_entity(removed));
    EXPECT_NE(reused, removed);
    world.AddComponent<Position>(reused, {2, -1});
    world.AddComponent<Sprite>(reused, {red.get()});
    world.AddComponent<RenderOrder>(reused, NextRenderOrder(registry, RenderLayer::Actors));
    EXPECT_EQ(drawPixel().r, 255);

    // Layer takes precedence over creation order, including changes made after a draw.
    world.GetComponent<RenderOrder>(reused).layer = RenderLayer::Terrain;
    EXPECT_EQ(drawPixel().b, 255);
    world.GetComponent<RenderOrder>(back).layer = RenderLayer::Effects;
    EXPECT_EQ(drawPixel().r, 255);
    world.RemoveComponent<RenderOrder>(back);
    EXPECT_EQ(drawPixel().b, 255);
    camera.CleanUp();
}
