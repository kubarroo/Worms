#pragma once
#include "Core/Physics/Collider.h"
#include "Game/Tags.h"
#include <box2d/b2_shape.h>
#include <box2d/b2_world.h>

class ColliderFactory
{
public:
    ColliderFactory(const ColliderFactory&) = delete;
    ColliderFactory(ColliderFactory&&) = delete;

    void Init(b2World* physicsWorld);

    static ColliderFactory& Get()
    {
        static ColliderFactory factory{};
        return factory;
    }

    Collider CreateTriggerBody(b2Shape* shape, b2Vec2 position, const PhysicsInfo& info = {});
    Collider CreateDynamicBody(b2Shape* shape, b2Vec2 position, const PhysicsInfo& info = {},
                               uintptr_t userData = 0);
    Collider CreateKineticBody(b2Shape* shape, b2Vec2 position, const PhysicsInfo& info = {});
    Collider CreateStaticBody(b2Shape* shape, b2Vec2 position, const PhysicsInfo& info = {});

    b2Fixture* CreateTriggerFixture(b2Body* body, b2Shape* shape, const PhysicsInfo& info = {});
    b2Fixture* CreateDynamicFixture(b2Body* body, b2Shape* shape, const PhysicsInfo& info = {});
    b2Fixture* CreateKineticFixture(b2Body* shape, b2Shape* body, const PhysicsInfo& info = {});
    b2Fixture* CreateStaticFixture(b2Body* body, b2Shape* shape, const PhysicsInfo& info = {});

    b2World* GetPhysicsWorld()
    {
        return physicsWorld;
    };

private:
    ColliderFactory() = default;

    b2World* physicsWorld;
};
