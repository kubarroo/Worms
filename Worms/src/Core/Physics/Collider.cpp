#include "Core/Physics/Collider.h"
#include "Core/Physics/ContactManager.h"
#include <utility>

SubscriptionId Collider::AddOnColliderEnter(CollisionCallback callback) const
{
    return ContactManager::Get().AddEvent(id, CollisionType::BEGIN, std::move(callback));
}

SubscriptionId Collider::AddOnColliderExit(CollisionCallback callback) const
{
    return ContactManager::Get().AddEvent(id, CollisionType::END, std::move(callback));
}

bool Collider::RemoveOnColliderEnter(SubscriptionId subscription) const
{
    return ContactManager::Get().RemoveEvent(subscription);
}

bool Collider::RemoveOnColliderExit(SubscriptionId subscription) const
{
    return ContactManager::Get().RemoveEvent(subscription);
}

void Collider::ClearOnColliderEnter() const
{
    ContactManager::Get().ClearEvent(id, CollisionType::BEGIN);
}

// void Collider::ClearOnCollider() const
//{
//	ContactManager::Get().ClearEvent( id, CollisionType::WHILE );
// }

void Collider::ClearOnColliderExit() const
{
    ContactManager::Get().ClearEvent(id, CollisionType::END);
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

Collider::Collider(b2Body* body, PhysicsInfo info)
{
    this->body = body;
    id = info.id;
}
