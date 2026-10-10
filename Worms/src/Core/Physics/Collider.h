#pragma once
#include "ECS/ECS_Types.h"
#include "Core/Physics/ContactManager.h"
#include "Game/Components.h"
#include "Game/Tags.h"
#include "box2d/b2_world.h"
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <functional>


class ColliderFactory;

class Collider
{
public:
    SubscriptionId AddOnColliderEnter(CollisionCallback callback) const;
    // void AddOnCollider( std::function<void( b2Contact* )> callback ) const;
    SubscriptionId AddOnColliderExit(CollisionCallback callback) const;

    bool RemoveOnColliderEnter(SubscriptionId subscription) const;
    // void RemoveOnCollider( std::function<void( b2Contact* )> callback ) const;
    bool RemoveOnColliderExit(SubscriptionId subscription) const;

    void ClearOnColliderEnter() const;
    // void ClearOnCollider() const;
    void ClearOnColliderExit() const;

    void FreezeRotation();

    void SetContinuous(bool isContinuous);
    void ApplyForce(b2Vec2 force);
    void SetVelocity(b2Vec2 velocity);

    b2Body* GetBody();

private:
    EntityId id;
    b2Body* body;
    ContactManager& contacts;

    friend ColliderFactory;
    Collider(b2Body*, PhysicsInfo, ContactManager&);
};
