#include "Core/Physics/ContactManager.h"
#include "Game/Tags.h"

void ContactManager::BeginContact(b2Contact* contact)
{
    beginContactFixtureUpdate(contact->GetFixtureA(), contact);
    beginContactFixtureUpdate(contact->GetFixtureB(), contact);
}

void ContactManager::EndContact(b2Contact* contact)
{
    endContactFixtureUpdate(contact->GetFixtureA(), contact);
    endContactFixtureUpdate(contact->GetFixtureB(), contact);
}

void ContactManager::beginContactFixtureUpdate(b2Fixture* fixture, b2Contact* contact)
{
    if (fixture->GetUserData().pointer != 0)
    {
        PhysicsInfo* info1 = (PhysicsInfo*)fixture->GetUserData().pointer;

        // if ( fixture->IsSensor() && updateEvents.contains( info1->id ) )
        //	updatableSensors.push_back( std::make_pair( info1->id, contact ) );

        if (beginEvents.contains(info1->id))
        {
            auto& evts = beginEvents[info1->id];
            for (const auto& evt : evts)
                evt(contact);
        }
    }
}

void ContactManager::endContactFixtureUpdate(b2Fixture* fixture, b2Contact* contact)
{
    if (fixture->GetUserData().pointer != 0)
    {
        PhysicsInfo* info1 = (PhysicsInfo*)fixture->GetUserData().pointer;
        if (beginEvents.contains(info1->id))
        {
            auto& evts = endEvents[info1->id];
            for (const auto& evt : evts)
                evt(contact);
        }
    }
}

void ContactManager::Update()
{
    // for ( auto [id, contact] : updatableSensors )
    //	for ( auto evt : updateEvents[id] )
    //		evt( contact );
}

void ContactManager::AddEvent(const EntityId entId, const CollisionType type,
                              std::function<void(b2Contact*)> evt)
{
    auto& events = GetEvents(type);
    auto iterator = events.find(entId);
    if (iterator == events.end())
    {
        events[entId].push_back(evt);
    }
}

void ContactManager::DeleteEvent(const EntityId entId, const CollisionType type,
                                 std::function<void(b2Contact*)> evt)
{
    auto& events = GetEvents(type);
    auto iterator = events.find(entId);
    if (iterator == events.end())
    {
        // Throw error
    }
    else
    {
        auto& vec = events[entId];
        std::erase_if(vec, [evt](const std::function<void(b2Contact*)>& other)
                      { return *(long*)(char*)&evt == *(long*)(char*)&other; });
        if (vec.size() == 0)
            events.erase(entId);
    }
}

void ContactManager::ClearEvent(const EntityId entId, const CollisionType type)
{
    auto& evts = GetEvents(type);
    auto iterator = evts.find(entId);
    if (iterator == evts.end())
    {
        // Throw error
    }
    else
    {
        auto& vec = evts[entId];
        vec.clear();
    }
}

EventMap& ContactManager::GetEvents(const CollisionType type)
{
    switch (type)
    {
    case CollisionType::BEGIN:
        return beginEvents;
    case CollisionType::WHILE_SENSOR_ONLY:
        return updateEvents;
    case CollisionType::END:
        return endEvents;
    }
}
