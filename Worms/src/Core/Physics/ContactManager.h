#pragma once
#include "ECS/ECS_Types.h"
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <box2d/box2d.h>
#include <functional>
#include <unordered_map>

enum CollisionType
{
    BEGIN,
    WHILE_SENSOR_ONLY,
    END
};

typedef std::vector<std::function<void(b2Contact*)>> CollisionEvent;
typedef std::unordered_map<EntityId, CollisionEvent> EventMap;

class ContactManager : public b2ContactListener
{
public:
    ContactManager(const ContactManager&) = delete;
    ContactManager(ContactManager&&) = delete;
    static ContactManager& Get()
    {
        static ContactManager singleton{};
        return singleton;
    }
    // Internal Use Only
    void BeginContact(b2Contact* contact);
    void EndContact(b2Contact* contact);

    void Update();

    void AddEvent(const EntityId entId, const CollisionType, std::function<void(b2Contact*)>);
    void DeleteEvent(const EntityId entId, const CollisionType, std::function<void(b2Contact*)>);
    void ClearEvent(const EntityId entId, const CollisionType);

    void ClearAll();

private:
    ContactManager() = default;
    EventMap& GetEvents(const CollisionType type);

    void beginContactFixtureUpdate(b2Fixture* fixture, b2Contact* contact);
    void endContactFixtureUpdate(b2Fixture* fixture, b2Contact* contact);

private:
    // std::vector<std::pair<EntityId, b2Contact*>> updatableSensors;

    EventMap beginEvents;
    EventMap updateEvents;
    EventMap endEvents;
};
