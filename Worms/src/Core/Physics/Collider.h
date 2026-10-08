#pragma once
#include "ECS/ECS_Types.h"
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
    void AddOnColliderEnter(std::function<void(b2Contact*)> callback) const;
    // void AddOnCollider( std::function<void( b2Contact* )> callback ) const;
    void AddOnColliderExit(std::function<void(b2Contact*)> callback) const;

    void RemoveOnColliderEnter(std::function<void(b2Contact*)> callback) const;
    // void RemoveOnCollider( std::function<void( b2Contact* )> callback ) const;
    void RemoveOnColliderExit(std::function<void(b2Contact*)> callback) const;

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

    friend ColliderFactory;
    Collider(b2Body*, PhysicsInfo);
};
