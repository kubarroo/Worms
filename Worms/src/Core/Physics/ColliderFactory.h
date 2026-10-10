#pragma once
#include "Core/Physics/Collider.h"
#include "Game/Tags.h"
#include <box2d/b2_shape.h>
#include <box2d/b2_world.h>
#include <stdexcept>

class ColliderFactory
{
public:
    ColliderFactory(const ColliderFactory&) = delete;
    ColliderFactory(ColliderFactory&&) = delete;

    ColliderFactory(b2World& physicsWorld, ContactManager& contacts)
        : physicsWorld(physicsWorld), contacts(contacts)
    {
    }
    void RequireServices(const b2World& expectedWorld, const ContactManager& expectedContacts) const
    {
        if (&physicsWorld != &expectedWorld || &contacts != &expectedContacts)
            throw std::invalid_argument("Scene context uses different physics services");
    }

    // Fixtures borrow info. Its address must remain stable until their bodies/fixtures are destroyed.
    Collider CreateTriggerBody(b2Shape* shape, b2Vec2 position, PhysicsInfo& info);
    Collider CreateDynamicBody(b2Shape* shape, b2Vec2 position, PhysicsInfo& info,
                               uintptr_t userData = 0);
    Collider CreateKineticBody(b2Shape* shape, b2Vec2 position, PhysicsInfo& info);
    Collider CreateStaticBody(b2Shape* shape, b2Vec2 position, PhysicsInfo& info);

    b2Fixture* CreateTriggerFixture(b2Body* body, b2Shape* shape, PhysicsInfo& info);
    b2Fixture* CreateDynamicFixture(b2Body* body, b2Shape* shape, PhysicsInfo& info);
    b2Fixture* CreateKineticFixture(b2Body* body, b2Shape* shape, PhysicsInfo& info);
    b2Fixture* CreateStaticFixture(b2Body* body, b2Shape* shape, PhysicsInfo& info);

    b2World& GetPhysicsWorld() const noexcept
    {
        return physicsWorld;
    }

private:
    b2World& physicsWorld;
    ContactManager& contacts;
};
