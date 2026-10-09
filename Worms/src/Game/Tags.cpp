#include "Tags.h"

std::optional<EntityId> GetEntityWithTag(b2Contact* contact, PhysicsTag tag)
{
    if (!contact) return {};
    for (auto* fixture : {contact->GetFixtureA(), contact->GetFixtureB()})
    {
        const auto* info = reinterpret_cast<const PhysicsInfo*>(fixture->GetUserData().pointer);
        if (info && info->tag == tag) return info->id;
    }
    return {};
}

std::optional<b2Body*> GetObjectWithTag(b2Contact* contact, PhysicsTag tag)
{
    if (!contact) return {};
    for (auto* fixture : {contact->GetFixtureA(), contact->GetFixtureB()})
    {
        const auto* info = reinterpret_cast<const PhysicsInfo*>(fixture->GetUserData().pointer);
        if (info && info->tag == tag) return fixture->GetBody();
    }
    return {};
}
