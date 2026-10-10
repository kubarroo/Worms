#pragma once
#include "Core/Audio/Sound.h"
#include "Core/GameObject.h"
#include "Core/SDLHandles.h"
#include "Core/Physics/Collider.h"
#include "Core/Physics/ColliderFactory.h"
#include "Game/Components.h"
#include "Game/Player/HealthBar.h"
#include "Game/Systems.h"
#include "Game/Tags.h"
#include <box2d/b2_world.h>
#include <memory>

class Worm : public GameObject
{
public:
    Worm(const Camera& camera, SDL_Texture* healthTexture, Position spawnPosition);
    void Initialise(const SceneContext& context) override;
    void Update(std::vector<Worm*>& wormsToDelete);
    void Jump();
    void CleanUp() override;
    void Render() override;

    void Activate();
    void Disactivate();
    bool IsGrounded() const;

private:
    SubscriptionId damageSubscription = 0;
    SubscriptionId groundedBeginSubscription = 0;
    SubscriptionId groundedEndSubscription = 0;
    const Camera& camera;
    // Borrowed from WormTeam, which owns this worm and its health bar.
    SDL_Texture* healthTexture;
    Position spawnPosition;
    // Shared assets borrowed from ResourceManager.
    Sound* jumpSound = nullptr;
    std::unique_ptr<HealthBar> healthBar;
    std::unique_ptr<Collider> collider = NULL;
    SDL_Texture* spriteTexture = nullptr;

    bool grounded = false;
    bool active = false;

    PhysicsInfo physicsInfo;
    PhysicsInfo groundedPhysicsInfo;
    EntityId groundedId{};
    bool hasGroundedEntity = false;
    static constexpr float WORM_SPEED = 0.8f;
    static constexpr float JUMP_FORCE = 2.5f;
};
