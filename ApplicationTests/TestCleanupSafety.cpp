#include "Core/Initialization/App.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Game/Tags.h"
#include "Game/Weapon/WeaponImpl.h"
#include <gtest/gtest.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
template <auto Method, typename... Args>
constexpr bool BorrowsNamedPhysicsInfo =
    std::is_invocable_v<decltype(Method), ColliderFactory&, Args..., PhysicsInfo&> &&
    !std::is_invocable_v<decltype(Method), ColliderFactory&, Args..., PhysicsInfo> &&
    !std::is_invocable_v<decltype(Method), ColliderFactory&, Args..., const PhysicsInfo&>;

static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateTriggerBody, b2Shape*, b2Vec2>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateKineticBody, b2Shape*, b2Vec2>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateStaticBody, b2Shape*, b2Vec2>);
static_assert(std::is_invocable_v<decltype(&ColliderFactory::CreateDynamicBody),
              ColliderFactory&, b2Shape*, b2Vec2, PhysicsInfo&, uintptr_t>);
static_assert(!std::is_invocable_v<decltype(&ColliderFactory::CreateDynamicBody),
              ColliderFactory&, b2Shape*, b2Vec2, PhysicsInfo, uintptr_t>);
static_assert(!std::is_invocable_v<decltype(&ColliderFactory::CreateDynamicBody),
              ColliderFactory&, b2Shape*, b2Vec2, const PhysicsInfo&, uintptr_t>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateTriggerFixture, b2Body*, b2Shape*>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateDynamicFixture, b2Body*, b2Shape*>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateKineticFixture, b2Body*, b2Shape*>);
static_assert(BorrowsNamedPhysicsInfo<&ColliderFactory::CreateStaticFixture, b2Body*, b2Shape*>);

TEST(ColliderFactorySafety, EveryBodyUsesRequestedPositionAndBorrowedMetadata)
{
    ContactManager contacts;
    PhysicsInfo info{PhysicsTag::NONE, 7};
    b2World world({0, 0});
    ColliderFactory factory(world, contacts);
    b2PolygonShape shape;
    shape.SetAsBox(0.5f, 0.5f);
    const b2Vec2 position{3.25f, -4.5f};
    Collider bodies[] = {
        factory.CreateTriggerBody(&shape, position, info),
        factory.CreateDynamicBody(&shape, position, info),
        factory.CreateKineticBody(&shape, position, info),
        factory.CreateStaticBody(&shape, position, info)
    };
    info.tag = PhysicsTag::WORM;
    for (auto& collider : bodies)
    {
        auto* body = collider.GetBody();
        EXPECT_FLOAT_EQ(body->GetPosition().x, position.x);
        EXPECT_FLOAT_EQ(body->GetPosition().y, position.y);
        ASSERT_NE(body->GetFixtureList(), nullptr);
        auto* metadata = reinterpret_cast<const PhysicsInfo*>(
            body->GetFixtureList()->GetUserData().pointer);
        ASSERT_EQ(metadata, &info);
        EXPECT_EQ(metadata->tag, PhysicsTag::WORM);
        EXPECT_EQ(metadata->id, info.id);
    }
}

TEST(ColliderFactorySafety, EveryFixtureBorrowsTheSuppliedMetadata)
{
    ContactManager contacts;
    PhysicsInfo info{PhysicsTag::NONE, 7};
    b2World world({0, 0});
    ColliderFactory factory(world, contacts);
    b2BodyDef bodyDef;
    auto* body = world.CreateBody(&bodyDef);
    b2PolygonShape shape;
    shape.SetAsBox(0.5f, 0.5f);
    b2Fixture* fixtures[] = {
        factory.CreateTriggerFixture(body, &shape, info),
        factory.CreateDynamicFixture(body, &shape, info),
        factory.CreateKineticFixture(body, &shape, info),
        factory.CreateStaticFixture(body, &shape, info)
    };
    for (auto* fixture : fixtures)
    {
        ASSERT_NE(fixture, nullptr);
        EXPECT_EQ(fixture->GetUserData().pointer, reinterpret_cast<uintptr_t>(&info));
    }
    EXPECT_TRUE(fixtures[0]->IsSensor());
}

TEST(CleanupSafety, WeaponConfigurationIsDestroyedThroughItsBase)
{
    struct TestConfiguration : WeaponImpl
    {
        explicit TestConfiguration(bool& destroyed) : destroyed(destroyed) {}
        ~TestConfiguration() override { destroyed = true; }
        bool& destroyed;
    };
    bool destroyed = false;
    {
        std::unique_ptr<WeaponImpl> config = std::make_unique<TestConfiguration>(destroyed);
        EXPECT_FALSE(destroyed);
    }
    EXPECT_TRUE(destroyed);
}

class ScopedHint
{
public:
    ScopedHint(const char* name, const char* value) : name(name)
    {
        if (const char* oldValue = SDL_GetHint(name))
            previous = oldValue;
        SDL_SetHintWithPriority(name, value, SDL_HINT_OVERRIDE);
    }
    ~ScopedHint()
    {
        SDL_ResetHint(name);
        if (previous)
            SDL_SetHintWithPriority(name, previous->c_str(), SDL_HINT_OVERRIDE);
    }
private:
    const char* name;
    std::optional<std::string> previous;
};

class TestApp : public App
{
public:
    bool HasWindow() const { return window != nullptr; }
    bool HasRenderer() const { return renderer != nullptr; }
    ~TestApp() override { Clean(); }
};

TEST(CleanupSafety, UninitializedAppCanBeCleanedTwice)
{
    TestApp app;
    EXPECT_NO_THROW(app.Clean());
    EXPECT_NO_THROW(app.Clean());
    EXPECT_FALSE(app.IsRunning());
    EXPECT_FALSE(app.HasWindow());
    EXPECT_FALSE(app.HasRenderer());
}

TEST(CleanupSafety, AppCanBeCleanedAfterVideoInitializationFails)
{
    ScopedHint videoDriver(SDL_HINT_VIDEODRIVER, "worms-nonexistent-driver");
    TestApp app;
    EXPECT_THROW(app.InitWindow("Test", 32, 32), SDL_Exception);
    EXPECT_NO_THROW(app.Clean());
    EXPECT_NO_THROW(app.Clean());
    EXPECT_FALSE(app.HasWindow());
    EXPECT_FALSE(app.HasRenderer());
    EXPECT_EQ(SDL_WasInit(0), 0u);
}

TEST(CleanupSafety, AppCanBeCleanedAfterRendererInitializationFails)
{
    ScopedHint videoDriver(SDL_HINT_VIDEODRIVER, "dummy");
    ScopedHint rendererDriver(SDL_HINT_RENDER_DRIVER, "worms-nonexistent-renderer");
    TestApp app;
    EXPECT_THROW(app.InitWindow("Test", 32, 32), SDL_Exception);
    EXPECT_TRUE(app.HasWindow());
    EXPECT_FALSE(app.HasRenderer());
    app.Clean();
    EXPECT_NO_THROW(app.Clean());
    EXPECT_FALSE(app.HasWindow());
    EXPECT_EQ(SDL_WasInit(0), 0u);
}

TEST(CleanupSafety, NegativeParticleCountIsRejectedBeforeCreatingEntities)
{
    EXPECT_THROW(ParticleSystem("unused.png", 1, 0, 0, -1), std::invalid_argument);
    ParticleSystem empty("unused.png", 1, 0, 0, 0);
    EXPECT_FALSE(empty.HasEntity());
    EXPECT_NO_THROW(empty.CleanUp());
}

class ContactCleanup : public testing::Test
{
protected:
    void SetUp() override
    {
        world.SetContactListener(&contacts);
        b2PolygonShape shape;
        shape.SetAsBox(0.5f, 0.5f);
        b2BodyDef fixed;
        auto* fixedBody = world.CreateBody(&fixed);
        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.isSensor = true;
        fixture.userData.pointer = reinterpret_cast<uintptr_t>(&info);
        fixedBody->CreateFixture(&fixture);
        b2BodyDef moving;
        moving.type = b2_dynamicBody;
        movingBody = world.CreateBody(&moving);
        movingBody->CreateFixture(&shape, 1);
        world.Step(1.f / 60, 8, 3);
    }
    void TearDown() override
    {
        world.SetContactListener(nullptr);
        contacts.ClearAll();
    }
    ContactManager contacts;
    PhysicsInfo info{PhysicsTag::NONE, 0};
    b2World world{b2Vec2{0, 0}};
    b2Body* movingBody = nullptr;
};

TEST_F(ContactCleanup, IndependentWorldsDispatchTheSameEntityIdToTheirOwnListeners)
{
    ContactManager otherContacts;
    b2World otherWorld({0, 0});
    otherWorld.SetContactListener(&otherContacts);
    ColliderFactory factory(world, contacts);
    ColliderFactory otherFactory(otherWorld, otherContacts);
    PhysicsInfo fixedInfo{PhysicsTag::NONE, info.id};
    PhysicsInfo movingInfo{PhysicsTag::NONE, static_cast<EntityId>(info.id + 1)};
    b2PolygonShape shape;
    shape.SetAsBox(0.5f, 0.5f);
    auto otherFixed = otherFactory.CreateStaticBody(&shape, {0, 0}, fixedInfo);
    auto otherMoving = otherFactory.CreateDynamicBody(&shape, {0, 0}, movingInfo);
    int firstCalls = 0, otherCalls = 0;
    contacts.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++firstCalls; });
    otherFixed.AddOnColliderEnter([&](b2Contact*) { ++otherCalls; });
    otherWorld.Step(1.f / 60, 8, 3);
    EXPECT_EQ(firstCalls, 0);
    EXPECT_EQ(otherCalls, 1);
    contacts.BeginContact(world.GetContactList());
    EXPECT_EQ(firstCalls, 1);
    EXPECT_EQ(otherCalls, 1);
    contacts.ClearAll();
    ASSERT_NE(otherWorld.GetContactList(), nullptr);
    otherContacts.BeginContact(otherWorld.GetContactList());
    EXPECT_EQ(otherCalls, 2);
    EXPECT_EQ(otherContacts.GetSubscriptionCount(), 1);
    EXPECT_THROW(factory.CreateStaticFixture(otherFixed.GetBody(), &shape, fixedInfo),
                 std::invalid_argument);
    otherFixed.ClearOnColliderEnter();
    EXPECT_EQ(otherContacts.GetSubscriptionCount(), 0);
    otherWorld.SetContactListener(nullptr);
    otherWorld.DestroyBody(otherMoving.GetBody());
    otherWorld.DestroyBody(otherFixed.GetBody());
}

TEST_F(ContactCleanup, PendingExceptionsAndClearAllAreLocalToEachManager)
{
    ContactManager other;
    contacts.AddEvent(info.id, BEGIN,
                      [](b2Contact*) { throw std::runtime_error("Local failure"); });
    other.AddEvent(info.id, BEGIN, [](b2Contact*) {});
    contacts.BeginContact(world.GetContactList());
    EXPECT_FALSE(other.TakePendingException());
    other.ClearAll();
    EXPECT_THROW(contacts.RethrowPendingException(), std::runtime_error);
    EXPECT_EQ(contacts.GetSubscriptionCount(), 1);
    EXPECT_EQ(other.GetSubscriptionCount(), 0);
}

TEST_F(ContactCleanup, ClearedEntityCanRegisterAnotherCallback)
{
    auto& manager = contacts;
    int oldCalls = 0;
    int newCalls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++oldCalls; });
    manager.ClearEvent(info.id, BEGIN);
    manager.ClearEvent(info.id, BEGIN);
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++newCalls; });
    ASSERT_NE(world.GetContactList(), nullptr);
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(oldCalls, 0);
    EXPECT_EQ(newCalls, 1);
}

TEST_F(ContactCleanup, EndCallbackDoesNotRequireBeginCallback)
{
    int endCalls = 0;
    contacts.AddEvent(info.id, END, [&](b2Contact*) { ++endCalls; });
    movingBody->SetTransform(b2Vec2{3, 0}, 0);
    world.Step(1.f / 60, 8, 3);
    EXPECT_EQ(endCalls, 1);
}

TEST_F(ContactCleanup, TagHelpersSkipMissingUserDataOnEitherFixture)
{
    auto* contact = world.GetContactList();
    ASSERT_NE(contact, nullptr);
    info.tag = WORM;
    auto* first = contact->GetFixtureA();
    auto* second = contact->GetFixtureB();
    first->GetUserData().pointer = 0;
    second->GetUserData().pointer = reinterpret_cast<uintptr_t>(&info);
    EXPECT_EQ(GetEntityWithTag(contact, WORM), std::optional<EntityId>{0});
    EXPECT_EQ(GetObjectWithTag(contact, WORM), std::optional<b2Body*>{second->GetBody()});
    EXPECT_FALSE(GetEntityWithTag(contact, DESTRUCTION_FIELD));
    EXPECT_FALSE(GetObjectWithTag(contact, DESTRUCTION_FIELD));

    first->GetUserData().pointer = reinterpret_cast<uintptr_t>(&info);
    second->GetUserData().pointer = 0;
    EXPECT_EQ(GetEntityWithTag(contact, WORM), std::optional<EntityId>{0});
    EXPECT_EQ(GetObjectWithTag(contact, WORM), std::optional<b2Body*>{first->GetBody()});
    EXPECT_FALSE(GetEntityWithTag(contact, DESTRUCTION_FIELD));
    EXPECT_FALSE(GetObjectWithTag(contact, DESTRUCTION_FIELD));
}

TEST_F(ContactCleanup, TagHelpersAcceptContactsWithoutAnyUserData)
{
    auto* contact = world.GetContactList();
    ASSERT_NE(contact, nullptr);
    contact->GetFixtureA()->GetUserData().pointer = 0;
    contact->GetFixtureB()->GetUserData().pointer = 0;
    EXPECT_FALSE(GetEntityWithTag(contact, WORM));
    EXPECT_FALSE(GetObjectWithTag(contact, WORM));
    EXPECT_FALSE(GetEntityWithTag(nullptr, WORM));
    EXPECT_FALSE(GetObjectWithTag(nullptr, WORM));
}

TEST_F(ContactCleanup, MultipleCallbacksRunInRegistrationOrder)
{
    auto& manager = contacts;
    std::vector<int> calls;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { calls.push_back(1); });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { calls.push_back(2); });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { calls.push_back(3); });
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, (std::vector<int>{1, 2, 3}));
}

TEST_F(ContactCleanup, RemovingOneCallbackPreservesOthersAndIsIdempotent)
{
    auto& manager = contacts;
    int removed = 0, remaining = 0;
    auto id = manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++removed; });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++remaining; });
    EXPECT_NE(id, 0u);
    EXPECT_TRUE(manager.RemoveEvent(id));
    EXPECT_FALSE(manager.RemoveEvent(id));
    EXPECT_FALSE(manager.RemoveEvent(0));
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(removed, 0);
    EXPECT_EQ(remaining, 1);
}

TEST_F(ContactCleanup, SelfRemovalKeepsExecutingCallbackAlive)
{
    auto& manager = contacts;
    auto state = std::make_shared<int>(0);
    std::weak_ptr<int> lifetime = state;
    SubscriptionId id = 0;
    id = manager.AddEvent(info.id, BEGIN, [&, state](b2Contact*)
    {
        EXPECT_TRUE(manager.RemoveEvent(id));
        EXPECT_FALSE(lifetime.expired());
        ++*state;
    });
    state.reset();
    manager.BeginContact(world.GetContactList());
    EXPECT_TRUE(lifetime.expired());
    EXPECT_NO_THROW(manager.BeginContact(world.GetContactList()));
}

TEST_F(ContactCleanup, CallbackCanRemoveAnotherPendingCallback)
{
    auto& manager = contacts;
    SubscriptionId second = 0;
    int calls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { manager.RemoveEvent(second); });
    second = manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 0);
}

TEST_F(ContactCleanup, AddedCallbackWaitsUntilNextDispatch)
{
    auto& manager = contacts;
    int calls = 0;
    bool added = false;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*)
    {
        if (!added)
        {
            added = true;
            manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
        }
    });
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 0);
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 1);
}

TEST_F(ContactCleanup, ClearingEntityDuringCallbackSkipsRemainingCallbacks)
{
    auto& manager = contacts;
    int beginCalls = 0, endCalls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { manager.ClearEvent(info.id, BEGIN); });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++beginCalls; });
    manager.AddEvent(info.id, END, [&](b2Contact*) { ++endCalls; });
    manager.BeginContact(world.GetContactList());
    manager.EndContact(world.GetContactList());
    EXPECT_EQ(beginCalls, 0);
    EXPECT_EQ(endCalls, 1);
}

TEST_F(ContactCleanup, ClearingAllDuringCallbackSkipsRemainingCallbacks)
{
    auto& manager = contacts;
    int calls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { manager.ClearAll(); });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    manager.AddEvent(info.id, END, [&](b2Contact*) { ++calls; });
    manager.BeginContact(world.GetContactList());
    manager.EndContact(world.GetContactList());
    EXPECT_EQ(calls, 0);
}

TEST_F(ContactCleanup, OldHandleAfterClearAllCannotRemoveNewSubscription)
{
    auto& manager = contacts;
    auto old = manager.AddEvent(info.id, BEGIN, [](b2Contact*) {});
    manager.ClearAll();
    int calls = 0;
    auto fresh = manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    EXPECT_NE(old, fresh);
    EXPECT_FALSE(manager.RemoveEvent(old));
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 1);
}

TEST_F(ContactCleanup, ClearAndRegisterDuringCallbackDoesNotRunReplacementImmediately)
{
    auto& manager = contacts;
    int calls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*)
    {
        manager.ClearAll();
        manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    });
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 0);
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 1);
}

TEST_F(ContactCleanup, MutableCallbackStatePersistsBetweenDispatches)
{
    auto& manager = contacts;
    int observed = 0;
    manager.AddEvent(info.id, BEGIN, [&, count = 0](b2Contact*) mutable { observed = ++count; });
    manager.BeginContact(world.GetContactList());
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(observed, 2);
}

TEST_F(ContactCleanup, InvalidCallbacksAndUnsupportedTypesAreRejected)
{
    auto& manager = contacts;
    EXPECT_THROW(manager.AddEvent(info.id, BEGIN, {}), std::invalid_argument);
    EXPECT_THROW(manager.AddEvent(info.id, WHILE_SENSOR_ONLY, [](b2Contact*) {}), std::invalid_argument);
    EXPECT_THROW(manager.AddEvent(info.id, static_cast<CollisionType>(99), [](b2Contact*) {}), std::invalid_argument);
    EXPECT_THROW(manager.ClearEvent(info.id, static_cast<CollisionType>(99)), std::invalid_argument);
}

TEST_F(ContactCleanup, NestedDispatchCanRemoveTheOuterPendingCallback)
{
    auto& manager = contacts;
    SubscriptionId second = 0;
    bool nested = false;
    int calls = 0;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact* contact)
    {
        if (!nested)
        {
            nested = true;
            manager.BeginContact(contact);
        }
        else manager.RemoveEvent(second);
    });
    second = manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    manager.BeginContact(world.GetContactList());
    EXPECT_EQ(calls, 0);
}

TEST_F(ContactCleanup, AddedCallbackForOtherFixtureWaitsUntilNextContactDispatch)
{
    auto& manager = contacts;
    auto* contact = world.GetContactList();
    ASSERT_NE(contact, nullptr);
    PhysicsInfo other{PhysicsTag::NONE, 1};
    contact->GetFixtureA()->GetUserData().pointer = reinterpret_cast<uintptr_t>(&info);
    contact->GetFixtureB()->GetUserData().pointer = reinterpret_cast<uintptr_t>(&other);
    int calls = 0;
    bool added = false;
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*)
    {
        if (!added)
        {
            added = true;
            manager.AddEvent(other.id, BEGIN, [&](b2Contact*) { ++calls; });
        }
    });
    manager.BeginContact(contact);
    EXPECT_EQ(calls, 0);
    manager.BeginContact(contact);
    EXPECT_EQ(calls, 1);
    contact->GetFixtureB()->GetUserData().pointer = 0;
}

TEST_F(ContactCleanup, ExceptionAfterSelfRemovalDoesNotCorruptRegistry)
{
    auto& manager = contacts;
    SubscriptionId id = 0;
    int calls = 0;
    id = manager.AddEvent(info.id, BEGIN, [&](b2Contact*)
    {
        manager.RemoveEvent(id);
        throw std::runtime_error("Callback failure");
    });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++calls; });
    EXPECT_NO_THROW(manager.BeginContact(world.GetContactList()));
    EXPECT_THROW(manager.RethrowPendingException(), std::runtime_error);
    EXPECT_NO_THROW(manager.RethrowPendingException());
    EXPECT_FALSE(manager.RemoveEvent(id));
    EXPECT_NO_THROW(manager.BeginContact(world.GetContactList()));
    EXPECT_EQ(calls, 2);
}

TEST_F(ContactCleanup, BeginExceptionDuringRealStepLeavesWorldUnlocked)
{
    auto& manager = contacts;
    movingBody->SetTransform(b2Vec2{3, 0}, 0);
    world.Step(1.f / 60, 8, 3);
    int remainingCalls = 0;
    manager.AddEvent(info.id, BEGIN, [](b2Contact*) { throw std::runtime_error("Begin failure"); });
    manager.AddEvent(info.id, BEGIN, [&](b2Contact*) { ++remainingCalls; });
    movingBody->SetTransform(b2Vec2{0, 0}, 0);
    // The first step discovers the new pair; the second updates its contact.
    EXPECT_NO_THROW(world.Step(1.f / 60, 8, 3));
    EXPECT_NO_THROW(world.Step(1.f / 60, 8, 3));
    EXPECT_FALSE(world.IsLocked());
    EXPECT_EQ(remainingCalls, 1);
    EXPECT_THROW(manager.RethrowPendingException(), std::runtime_error);
    EXPECT_NO_THROW(manager.RethrowPendingException());
    EXPECT_NO_THROW(world.DestroyBody(movingBody));
    movingBody = nullptr;
}

TEST_F(ContactCleanup, EndExceptionDuringRealStepLeavesWorldUnlocked)
{
    auto& manager = contacts;
    manager.AddEvent(info.id, END, [](b2Contact*) { throw std::runtime_error("End failure"); });
    movingBody->SetTransform(b2Vec2{3, 0}, 0);
    EXPECT_NO_THROW(world.Step(1.f / 60, 8, 3));
    EXPECT_FALSE(world.IsLocked());
    EXPECT_THROW(manager.RethrowPendingException(), std::runtime_error);
    EXPECT_NO_THROW(world.DestroyBody(movingBody));
    movingBody = nullptr;
}

TEST_F(ContactCleanup, EndExceptionDuringBodyDestructionDoesNotInterruptCleanup)
{
    auto& manager = contacts;
    int remainingCalls = 0;
    manager.AddEvent(info.id, END, [](b2Contact*) { throw std::runtime_error("Destroy failure"); });
    manager.AddEvent(info.id, END, [&](b2Contact*) { ++remainingCalls; });
    EXPECT_NO_THROW(world.DestroyBody(movingBody));
    movingBody = nullptr;
    EXPECT_FALSE(world.IsLocked());
    EXPECT_EQ(world.GetBodyCount(), 1);
    EXPECT_EQ(remainingCalls, 1);
    EXPECT_THROW(manager.RethrowPendingException(), std::runtime_error);
    EXPECT_NO_THROW(manager.ClearAll());
    EXPECT_NO_THROW(manager.ClearAll());
    EXPECT_NO_THROW(manager.RethrowPendingException());
}

TEST_F(ContactCleanup, FirstExceptionIsPreservedUntilConsumedEvenAfterClearAll)
{
    auto& manager = contacts;
    manager.AddEvent(info.id, BEGIN, [](b2Contact*) { throw std::logic_error("First failure"); });
    manager.AddEvent(info.id, BEGIN, [](b2Contact*) { throw std::runtime_error("Second failure"); });
    EXPECT_NO_THROW(manager.BeginContact(world.GetContactList()));
    manager.ClearAll();
    EXPECT_THROW(manager.RethrowPendingException(), std::logic_error);
    EXPECT_NO_THROW(manager.RethrowPendingException());
}

TEST_F(ContactCleanup, NonStandardExceptionIsDeferredAndCanBeConsumed)
{
    auto& manager = contacts;
    manager.AddEvent(info.id, END, [](b2Contact*) { throw 42; });
    EXPECT_NO_THROW(world.DestroyBody(movingBody));
    movingBody = nullptr;
    EXPECT_FALSE(world.IsLocked());
    auto error = manager.TakePendingException();
    ASSERT_TRUE(error);
    EXPECT_THROW(std::rethrow_exception(error), int);
    EXPECT_FALSE(manager.TakePendingException());
}
}
