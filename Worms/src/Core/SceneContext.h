#pragma once
#include <memory>
#include <entt/entity/registry.hpp>

struct SDL_Renderer;
class b2World;
class GameObject;
class ColliderFactory;
class ContactManager;
class ResourceManager;
class Renderer2D;
class Audio;
class Input;

// Commands borrow their recipient; QueueAdd and RetireObject transfer ownership.
class ObjectCommands
{
public:
    virtual ~ObjectCommands() = default;
    virtual void QueueAdd(std::unique_ptr<GameObject> object) = 0;
    virtual void RequestDestroy(GameObject& object) = 0;
    // Keeps ownership until cleanup can safely release physics and ECS resources.
    virtual void RetireObject(std::unique_ptr<GameObject> object) = 0;
};

// Non-owning dependencies. The renderer may be null for headless ECS tests.
// All services must outlive every object initialized with this context.
struct SceneContext
{
    SDL_Renderer* renderer;
    entt::registry& registry;
    b2World& physics;
    ObjectCommands& objects;
    ColliderFactory& colliders;
    ContactManager& contacts;
    ResourceManager& resources;
    Renderer2D& rendering;
    Audio& audio;
    Input& input;
};
