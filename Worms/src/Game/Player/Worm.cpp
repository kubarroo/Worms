#include "Game/Player/Worm.h"
#include "Core/Input.h"
#include "Core/Audio/Audio.h"
#include "Core/Renderer2D.h"
#include "Core/ResourceManager.h"
#include "Core/ParticleSystem.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Core/Utils.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <box2d/b2_contact.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_polygon_shape.h>

Worm::Worm(const Camera& camera, SDL_Texture* healthTexture, Position spawnPosition)
    : camera(camera), healthTexture(healthTexture), spawnPosition(spawnPosition)
{
}

void Worm::Initialise(const SceneContext& context)
{
    context.colliders.RequireServices(context.physics, context.contacts);
    GameObject::Initialise(context);
    try
    {
        jumpSound = &context.resources.GetSound("jump.wav");
        auto& pos = world->AddComponent<Position>(objectId, Position{spawnPosition.x, spawnPosition.y});

        physicsInfo.tag = PhysicsTag::WORM;
        physicsInfo.id = objectId;

        damageSubscription = Context().contacts.AddEvent(
            objectId, CollisionType::BEGIN,
            [&](b2Contact* contact)
            {
                auto entId = GetEntityWithTag(contact, PhysicsTag::DESTRUCTION_FIELD);
                if (entId.has_value())
                    healthBar->TakeDamage(40);
            });

        groundedId = world->CreateEntity();
        hasGroundedEntity = true;
        groundedPhysicsInfo.tag = PhysicsTag::GROUNDED;
        groundedPhysicsInfo.id = groundedId;

        Sprite& spriteComponent = world->AddComponent<Sprite>(objectId);
        spriteTexture = context.resources.GetTexture("worms.png");
        spriteComponent.texture = spriteTexture;

        b2PolygonShape shape;

        shape.SetAsBox(0.07, 0.1);
        b2PolygonShape groundShape;
        groundShape.SetAsBox(0.075, 0.07, {0.f, -0.08f}, 0.f);

        auto createdCollider =
            Context().colliders.CreateDynamicBody(&shape, {pos.x, pos.y}, physicsInfo);
        try { collider = std::make_unique<Collider>(std::move(createdCollider)); }
        catch (...)
        {
            createdCollider.GetBody()->GetWorld()->DestroyBody(createdCollider.GetBody());
            throw;
        }
        collider->FreezeRotation();
        Context().colliders.CreateTriggerFixture(collider->GetBody(), &groundShape,
                                                 groundedPhysicsInfo);
        groundedBeginSubscription = Context().contacts.AddEvent(
            groundedId, CollisionType::BEGIN, [&](b2Contact*) { grounded = true; });
        groundedEndSubscription = Context().contacts.AddEvent(
            groundedId, CollisionType::END, [&](b2Contact*) { grounded = false; });

        healthBar = std::make_unique<HealthBar>(objectId, camera, 100, healthTexture);
        healthBar->Initialise(context);
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
        Context().objects.QueueAdd(
            std::make_unique<ParticleSystem>("blood.png", 2.f, pos.x, pos.y, 200));
        wormsToDelete.emplace_back(this);
    }
    if (!active)
    {
        return;
    }

    if (abs(rb.body->GetLinearVelocity().x) < 2)
        rb.body->SetLinearVelocity(
            {Context().input.Horizontal() * WORM_SPEED, rb.body->GetLinearVelocity().y});

    Jump();
}

void Worm::Jump()
{
    if (!HasEntity() || !collider)
    {
        return;
    }
    auto& rb = world->GetComponent<RigidBody>(objectId);
    if (!IsGrounded() || !Context().input.Jump() || rb.body->GetLinearVelocity().y > 0.4)
    {
        return;
    }

    grounded = false;
    rb.body->SetLinearVelocity(
        {rb.body->GetLinearVelocity().x * sqrtf(2.0), JUMP_FORCE * sqrtf(2.0)});
    Context().audio.Play(*jumpSound);
}

void Worm::CleanUp()
{
    if (!HasEntity())
    {
        return;
    }
    Context().contacts.RemoveEvent(damageSubscription);
    Context().contacts.RemoveEvent(groundedBeginSubscription);
    Context().contacts.RemoveEvent(groundedEndSubscription);
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
        healthBar.reset();
    }
    GameObject::CleanUp();
    spriteTexture = nullptr;
    jumpSound = nullptr;
    active = grounded = false;
}

void Worm::Render()
{
    if (HasEntity() && healthBar) healthBar->Render();
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
