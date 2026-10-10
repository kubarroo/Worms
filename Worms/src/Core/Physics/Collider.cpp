#include "Core/Physics/Collider.h"
#include "Core/Physics/ContactManager.h"
#include <utility>

SubscriptionId Collider::AddOnColliderEnter(CollisionCallback callback) const
{
    return contacts.AddEvent(id, CollisionType::BEGIN, std::move(callback));
}

SubscriptionId Collider::AddOnColliderExit(CollisionCallback callback) const
{
    return contacts.AddEvent(id, CollisionType::END, std::move(callback));
}

bool Collider::RemoveOnColliderEnter(SubscriptionId subscription) const
{
    return contacts.RemoveEvent(subscription);
}

bool Collider::RemoveOnColliderExit(SubscriptionId subscription) const
{
    return contacts.RemoveEvent(subscription);
}

void Collider::ClearOnColliderEnter() const
{
    contacts.ClearEvent(id, CollisionType::BEGIN);
}

void Collider::ClearOnColliderExit() const
{
    contacts.ClearEvent(id, CollisionType::END);
}

void Collider::FreezeRotation()
{
    body->SetFixedRotation(true);
}

void Collider::SetContinuous(bool isContinuous)
{
    body->SetBullet(isContinuous);
}

void Collider::ApplyForce(b2Vec2 force)
{
    body->ApplyForceToCenter(force, true);
}

void Collider::SetVelocity(b2Vec2 velocity)
{
    body->ApplyLinearImpulseToCenter(velocity, true);
}

b2Body* Collider::GetBody()
{
    return body;
}

Collider::Collider(b2Body* body, PhysicsInfo info, ContactManager& contacts)
    : id(info.id), body(body), contacts(contacts)
{
}
