#include "ColliderFactory.h"
#include "Game/Components.h"

void ColliderFactory::Init(b2World* physicsWorld)
{
    this->physicsWorld = physicsWorld;
}

Collider ColliderFactory::CreateTriggerBody(b2Shape* shape, b2Vec2 position,
                                            const PhysicsInfo& info)
{
    b2BodyDef bodyDef;
    bodyDef.type = b2_kinematicBody;
    bodyDef.fixedRotation = true;
    b2Body* body = physicsWorld->CreateBody(&bodyDef);
    CreateTriggerFixture(body, shape, info);

    return Collider(body, info);
}

Collider ColliderFactory::CreateDynamicBody(b2Shape* shape, b2Vec2 position,
                                            const PhysicsInfo& info, uintptr_t userData)
{
    static b2BodyDef bodyDef;
    bodyDef.position = position;
    bodyDef.type = b2_dynamicBody;
    bodyDef.angularDamping = 1.0f;
    bodyDef.userData.pointer = userData;
    b2Body* body = physicsWorld->CreateBody(&bodyDef);
    CreateDynamicFixture(body, shape, info);

    return std::move(Collider(body, info));
}

Collider ColliderFactory::CreateKineticBody(b2Shape* shape, b2Vec2 position,
                                            const PhysicsInfo& info)
{
    b2BodyDef bodyDef;
    bodyDef.type = b2_kinematicBody;
    bodyDef.fixedRotation = true;
    b2Body* body = physicsWorld->CreateBody(&bodyDef);
    CreateKineticFixture(body, shape, info);

    return Collider(body, info);
}

Collider ColliderFactory::CreateStaticBody(b2Shape* shape, b2Vec2 position, const PhysicsInfo& info)
{
    b2BodyDef bodyDef;
    bodyDef.type = b2_staticBody;
    bodyDef.fixedRotation = true;
    bodyDef.position = position;
    b2Body* body = physicsWorld->CreateBody(&bodyDef);
    CreateStaticFixture(body, shape, info);

    return Collider(body, info);
}

b2Fixture* ColliderFactory::CreateTriggerFixture(b2Body* body, b2Shape* shape,
                                                 const PhysicsInfo& info)
{
    b2FixtureDef fixtureDef;
    fixtureDef.isSensor = true;
    fixtureDef.shape = shape;
    fixtureDef.friction = 1;
    fixtureDef.density = 1;
    fixtureDef.userData.pointer = reinterpret_cast<uintptr_t>(&info);
    return body->CreateFixture(&fixtureDef);
}

b2Fixture* ColliderFactory::CreateDynamicFixture(b2Body* body, b2Shape* shape,
                                                 const PhysicsInfo& info)
{
    b2FixtureDef fixtureDef;
    fixtureDef.shape = shape;
    fixtureDef.friction = 1;
    fixtureDef.density = 1;
    fixtureDef.userData.pointer = reinterpret_cast<uintptr_t>(&info);
    return body->CreateFixture(&fixtureDef);
}

b2Fixture* ColliderFactory::CreateKineticFixture(b2Body* body, b2Shape* shape,
                                                 const PhysicsInfo& info)
{
    b2FixtureDef fixtureDef;
    fixtureDef.shape = shape;
    fixtureDef.friction = 1;
    fixtureDef.density = 1;
    fixtureDef.userData.pointer = reinterpret_cast<uintptr_t>(&info);
    return body->CreateFixture(&fixtureDef);
}

b2Fixture* ColliderFactory::CreateStaticFixture(b2Body* body, b2Shape* shape,
                                                const PhysicsInfo& info)
{
    b2FixtureDef fixtureDef;
    fixtureDef.shape = shape;
    fixtureDef.userData.pointer = reinterpret_cast<uintptr_t>(&info);
    return body->CreateFixture(&fixtureDef);
}
