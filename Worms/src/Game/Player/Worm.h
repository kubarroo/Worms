#pragma once
#include "Core/Audio/Sound.h"
#include "Core/GameObject.h"
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
    using TexturePtr = std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)>;
    Worm(SDL_Renderer* newRenderer, World* newWorld, b2World* physicsWorld, const Camera& camera,
         SDL_Texture* texture);
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
    Sound jumpSound{"jump.wav"};
    std::unique_ptr<HealthBar> healthBar;
    std::unique_ptr<Collider> collider = NULL;
    TexturePtr spriteTexture{nullptr, &SDL_DestroyTexture};

    bool grounded = false;
    bool active = false;

    PhysicsInfo physicsInfo;
    PhysicsInfo groundedPhysicsInfo;
    EntityId groundedId{};
    bool hasGroundedEntity = false;
    static constexpr float WORM_SPEED = 0.8f;
    static constexpr float JUMP_FORCE = 2.5f;
};
