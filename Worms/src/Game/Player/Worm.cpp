#include "Game/Player/Worm.h"
#include "Core/Input.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Core/Utils.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_polygon_shape.h>

Worm::Worm(SDL_Renderer* newRenderer, World* newWorld, b2World* physicsWorld, const Camera& camera,
           SDL_Texture* texture)
{
    try
    {
        Initialise(newRenderer, newWorld);
        static float posX = -2.f;
        posX += 1.f;
        auto& pos = world->AddComponent<Position>(objectId, {posX, 2});

        physicsInfo.tag = PhysicsTag::WORM;
        physicsInfo.id = objectId;

        damageSubscription = ContactManager::Get().AddEvent(objectId, CollisionType::BEGIN,
                                       [&](b2Contact* contact)
                                       {
                                           auto entId = GetEntityWithTag(
                                               contact, PhysicsTag::DESTRUCTION_FIELD);
                                           if (entId.has_value())
                                               healthBar->TakeDamage(40);
                                       });

        groundedId = world->CreateEntity();
        hasGroundedEntity = true;
        groundedPhysicsInfo.tag = PhysicsTag::GROUNDED;
        groundedPhysicsInfo.id = groundedId;

        Sprite& spriteComponent = world->AddComponent<Sprite>(objectId);
        spriteTexture.reset(IMG_LoadTexture(renderer, "worms.png"));
        SDL_CHECK(spriteTexture.get());
        spriteComponent.texture = spriteTexture.get();

        b2PolygonShape shape;

        shape.SetAsBox(0.07, 0.1);
        b2PolygonShape groundShape;
        groundShape.SetAsBox(0.075, 0.07, {0.f, -0.08f}, 0.f);

        collider = std::make_unique<Collider>(
            ColliderFactory::Get().CreateDynamicBody(&shape, {pos.x, pos.y}, physicsInfo));
        collider->FreezeRotation();
        ColliderFactory::Get().CreateTriggerFixture(collider->GetBody(), &groundShape,
                                                    groundedPhysicsInfo);
        groundedBeginSubscription = ContactManager::Get().AddEvent(groundedId, CollisionType::BEGIN,
                                       [&](b2Contact*) { grounded = true; });
        groundedEndSubscription = ContactManager::Get().AddEvent(groundedId, CollisionType::END,
                                       [&](b2Contact*) { grounded = false; });

        healthBar =
            std::make_unique<HealthBar>(newRenderer, newWorld, objectId, camera, 100, texture);
        world->AddComponent<RigidBody>(objectId, {collider->GetBody()});
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}

void Worm::Update(std::vector<Worm*>& wormsToDelete)
{
    if (!HasEntity() || !collider || !healthBar)
    {
        return;
    }
    auto& pos = world->GetComponent<Position>(objectId);

    if (pos.y < -15.f)
        healthBar->TakeDamage(100);
    auto& rb = world->GetComponent<RigidBody>(objectId);
    healthBar->Update();

    if (healthBar->getCurrentHp() <= 0)
    {
        GameObject::objsToAdd.emplace_back(
            std::make_unique<ParticleSystem>("blood.png", 2.f, pos.x, pos.y, 200));
        wormsToDelete.emplace_back(this);
    }
    if (!active)
    {
        return;
    }

    if (abs(rb.body->GetLinearVelocity().x) < 2)
        rb.body->SetLinearVelocity(
            {Input::Get().Horizontal() * WORM_SPEED, rb.body->GetLinearVelocity().y});

    Jump();
}

void Worm::Jump()
{
    if (!HasEntity() || !collider)
    {
        return;
    }
    auto& rb = world->GetComponent<RigidBody>(objectId);
    if (!IsGrounded() || !Input::Get().Jump() || rb.body->GetLinearVelocity().y > 0.4)
    {
        return;
    }

    grounded = false;
    rb.body->SetLinearVelocity(
        {rb.body->GetLinearVelocity().x * sqrtf(2.0), JUMP_FORCE * sqrtf(2.0)});
    jumpSound.Play();
}

void Worm::CleanUp()
{
    if (!HasEntity())
    {
        return;
    }
    ContactManager::Get().RemoveEvent(damageSubscription);
    ContactManager::Get().RemoveEvent(groundedBeginSubscription);
    ContactManager::Get().RemoveEvent(groundedEndSubscription);
    damageSubscription = groundedBeginSubscription = groundedEndSubscription = 0;
    if (collider)
    {
        collider->GetBody()->GetWorld()->DestroyBody(collider->GetBody());
        collider.reset();
    }
    if (hasGroundedEntity)
    {
        world->DestroyEntity(groundedId);
        hasGroundedEntity = false;
        groundedId = {};
    }
    if (healthBar)
    {
        healthBar->CleanUp();
    }
    GameObject::CleanUp();
    spriteTexture.reset();
}

void Worm::Render()
{
    healthBar->Render();
}

void Worm::Activate()
{
    active = true;
}

void Worm::Disactivate()
{
    active = false;
}

bool Worm::IsGrounded() const
{
    return grounded;
}
