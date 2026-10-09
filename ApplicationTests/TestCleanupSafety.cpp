#include "Core/Initialization/App.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ContactManager.h"
#include "Game/Weapon/WeaponImpl.h"
#include <gtest/gtest.h>
#include <optional>
#include <memory>
#include <string>

namespace
{
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
        ContactManager::Get().ClearAll();
        world.SetContactListener(&ContactManager::Get());
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
        ContactManager::Get().ClearAll();
    }
    PhysicsInfo info{PhysicsTag::NONE, 0};
    b2World world{b2Vec2{0, 0}};
    b2Body* movingBody = nullptr;
};

TEST_F(ContactCleanup, ClearedEntityCanRegisterAnotherCallback)
{
    auto& manager = ContactManager::Get();
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
    ContactManager::Get().AddEvent(info.id, END, [&](b2Contact*) { ++endCalls; });
    movingBody->SetTransform(b2Vec2{3, 0}, 0);
    world.Step(1.f / 60, 8, 3);
    EXPECT_EQ(endCalls, 1);
}
}
