#pragma once
#include "ECS/ECS_Types.h"
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <box2d/box2d.h>
#include <functional>
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <vector>
#include <exception>

enum CollisionType
{
    BEGIN,
    WHILE_SENSOR_ONLY,
    END
};

using SubscriptionId = std::uint64_t;
using CollisionCallback = std::function<void(b2Contact*)>;
typedef std::vector<SubscriptionId> CollisionEvent;
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
    void BeginContact(b2Contact* contact) noexcept override;
    void EndContact(b2Contact* contact) noexcept override;

    void Update();

    // Registrations added during dispatch wait for the next contact notification.
    SubscriptionId AddEvent(EntityId entId, CollisionType type, CollisionCallback callback);
    // Removing an executing callback keeps it alive until its invocation returns.
    bool RemoveEvent(SubscriptionId subscription);
    void ClearEvent(const EntityId entId, const CollisionType);

    void ClearAll();
    // Consume errors only outside Box2D callbacks, after the world is unlocked.
    std::exception_ptr TakePendingException() noexcept;
    void RethrowPendingException();

private:
    ContactManager() = default;
    EventMap& GetEvents(const CollisionType type);
    CollisionEvent Pending(b2Fixture* fixture, CollisionType type);
    void Dispatch(const CollisionEvent& pending, b2Contact* contact);
    void HandleContact(b2Contact* contact, CollisionType type) noexcept;

private:
    struct Subscription
    {
        EntityId entity;
        CollisionType type;
        CollisionCallback callback;
    };
    SubscriptionId nextSubscription = 1;
    std::exception_ptr pendingException;
    std::unordered_map<SubscriptionId, std::shared_ptr<Subscription>> subscriptions;

    EventMap beginEvents;
    EventMap endEvents;
};
