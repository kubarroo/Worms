#pragma once
#include <memory>

struct SDL_Renderer;
class World;
class b2World;
class GameObject;
class ColliderFactory;
class ContactManager;

// Commands borrow their recipient; ownership transfers only through QueueAdd.
class ObjectCommands
{
public:
    virtual ~ObjectCommands() = default;
    virtual void QueueAdd(std::unique_ptr<GameObject> object) = 0;
    virtual void RequestDestroy(GameObject& object) = 0;
};

// Non-owning dependencies. The renderer may be null for headless ECS tests.
// Worlds and commands must outlive every object initialized with this context.
struct SceneContext
{
    SDL_Renderer* renderer;
    World& world;
    b2World& physics;
    ObjectCommands& objects;
    ColliderFactory& colliders;
    ContactManager& contacts;
};
