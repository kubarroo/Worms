#include "Core/Physics/ContactManager.h"
#include "Game/Tags.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

void ContactManager::BeginContact(b2Contact* contact) noexcept
{
    HandleContact(contact, BEGIN);
}

void ContactManager::EndContact(b2Contact* contact) noexcept
{
    HandleContact(contact, END);
}

void ContactManager::HandleContact(b2Contact* contact, CollisionType type) noexcept
{
    try
    {
        const auto first = Pending(contact->GetFixtureA(), type);
        const auto second = Pending(contact->GetFixtureB(), type);
        Dispatch(first, contact);
        Dispatch(second, contact);
    }
    catch (...)
    {
        if (!pendingException) pendingException = std::current_exception();
    }
}

CollisionEvent ContactManager::Pending(b2Fixture* fixture, CollisionType type)
{
    if (!fixture->GetUserData().pointer) return {};
    const auto* info = reinterpret_cast<const PhysicsInfo*>(fixture->GetUserData().pointer);
    auto& events = GetEvents(type);
    auto found = events.find(info->id);
    if (found == events.end()) return {};
    return found->second;
}

void ContactManager::Dispatch(const CollisionEvent& pending, b2Contact* contact)
{
    for (auto id : pending)
    {
        auto current = subscriptions.find(id);
        if (current == subscriptions.end()) continue;
        // Keep the executing callable alive if it removes its own registration.
        auto subscription = current->second;
        try
        {
            subscription->callback(contact);
        }
        catch (...)
        {
            if (!pendingException) pendingException = std::current_exception();
        }
    }
}

void ContactManager::Update() {}

std::exception_ptr ContactManager::TakePendingException() noexcept
{
    return std::exchange(pendingException, nullptr);
}

void ContactManager::RethrowPendingException()
{
    if (auto error = TakePendingException()) std::rethrow_exception(error);
}

SubscriptionId ContactManager::AddEvent(EntityId entity, CollisionType type,
                                        CollisionCallback callback)
{
    auto& events = GetEvents(type);
    if (!callback) throw std::invalid_argument("Collision callback is empty");
    if (!nextSubscription) throw std::overflow_error("Collision subscription IDs exhausted");
    const auto id = nextSubscription++;
    subscriptions.emplace(id, std::make_shared<Subscription>(
        Subscription{entity, type, std::move(callback)}));
    try
    {
        events[entity].push_back(id);
    }
    catch (...)
    {
        subscriptions.erase(id);
        auto found = events.find(entity);
        if (found != events.end() && found->second.empty()) events.erase(found);
        throw;
    }
    return id;
}

bool ContactManager::RemoveEvent(SubscriptionId id)
{
    auto found = subscriptions.find(id);
    if (found == subscriptions.end()) return false;
    auto& events = GetEvents(found->second->type);
    auto bucket = events.find(found->second->entity);
    if (bucket != events.end())
    {
        std::erase(bucket->second, id);
        if (bucket->second.empty()) events.erase(bucket);
    }
    subscriptions.erase(found);
    return true;
}

void ContactManager::ClearEvent(EntityId entity, CollisionType type)
{
    auto& events = GetEvents(type);
    auto found = events.find(entity);
    if (found == events.end()) return;
    for (auto id : found->second) subscriptions.erase(id);
    events.erase(found);
}

EventMap& ContactManager::GetEvents(CollisionType type)
{
    switch (type)
    {
    case BEGIN: return beginEvents;
    case END: return endEvents;
    case WHILE_SENSOR_ONLY:
        throw std::invalid_argument("Continuous sensor callbacks are not implemented");
    default: throw std::invalid_argument("Invalid collision type");
    }
}

void ContactManager::ClearAll()
{
    beginEvents.clear();
    endEvents.clear();
    subscriptions.clear();
}
