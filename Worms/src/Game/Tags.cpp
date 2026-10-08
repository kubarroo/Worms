#include "Tags.h"

std::optional<EntityId> GetEntityWithTag(b2Contact* contact, PhysicsTag tag)
{
    if (reinterpret_cast<PhysicsInfo*>(contact->GetFixtureA()->GetUserData().pointer)->tag == tag)
        return reinterpret_cast<PhysicsInfo*>(contact->GetFixtureA()->GetUserData().pointer)->id;
    else if (reinterpret_cast<PhysicsInfo*>(contact->GetFixtureB()->GetUserData().pointer)->tag ==
             tag)
        return reinterpret_cast<PhysicsInfo*>(contact->GetFixtureB()->GetUserData().pointer)->id;
    else
        return {};
}

std::optional<b2Body*> GetObjectWithTag(b2Contact* contact, PhysicsTag tag)
{
    if (reinterpret_cast<PhysicsInfo*>(contact->GetFixtureA()->GetUserData().pointer)->tag == tag)
        return contact->GetFixtureA()->GetBody();
    else if (reinterpret_cast<PhysicsInfo*>(contact->GetFixtureB()->GetUserData().pointer)->tag ==
             tag)
        return contact->GetFixtureB()->GetBody();
    else
        return {};
}