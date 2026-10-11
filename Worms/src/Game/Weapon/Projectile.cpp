#include <box2d/b2_body.h>
#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world.h>
#include <SDL3_image/SDL_image.h>
#include "Core/ParticleSystem.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Weapon/Projectile.h"
#include "Core/Audio/Audio.h"
#include "Game/RenderOrder.h"

Projectile::Projectile( float posX, float posY, float vX, float vY ) : startPosX( posX ), startPosY( posY ), startVelX( vX ), startVelY( vY )
{
}

void Projectile::Update()
{
	if ( !HasEntity() || !collider ) return;
	auto& pos = Registry().get<Position>( objectId );
	if ( createSensor ||
		 pos.y < -15.f ||
		 timer.Measure() > params.explosionOffset && params.explosionOffset != 0 )
	{
		createSensor = false;
		if ( explosionSound ) Context().audio.Play(*explosionSound);
        Context().objects.QueueAdd(std::make_unique<ParticleSystem>(
            "particle.png", params.explosionRadius * 3.f, pos.x, pos.y, 100));

        Registry().get<RigidBody>(objectId).body->SetAwake(true);
        sensorInfo = {PhysicsTag::DESTRUCTION_FIELD, objectId, &Context().physics};
		b2CircleShape shape;
		shape.m_radius = params.explosionRadius;
        fixture = Context().colliders.CreateTriggerFixture(collider->GetBody(), &shape, sensorInfo);
        Context().objects.RequestDestroy(*this);
    }
}

void Projectile::CleanUp()
{
    if (collisionSubscription)
        Context().contacts.RemoveEvent(collisionSubscription);
    collisionSubscription = 0;
	if ( collider )
	{
		collider->GetBody()->GetWorld()->DestroyBody( collider->GetBody() );
		collider.reset();
	}
	fixture = nullptr;
	camera = nullptr;
	GameObject::CleanUp();
    texture = nullptr;
    explosionSound = collisionSound = nullptr;
    createSensor = false;
}

void Projectile::onCollision( b2Contact* constact )
{
    Context().contacts.RemoveEvent(collisionSubscription);
    collisionSubscription = 0;
	if ( collisionSound ) Context().audio.Play(*collisionSound);
	if ( params.explosionOffset == 0 )
		createSensor = true;

}

void Projectile::Initialise(const SceneContext& context)
{
    context.colliders.RequireServices(context.physics, context.contacts);
    GameObject::Initialise(context);
    try
    {
        timer.Reset();
        Registry().emplace<Position>(objectId, Position{startPosX, startPosY});
        Registry().emplace<RenderOrder>(objectId,
            NextRenderOrder(context.registry, RenderLayer::Projectiles));
        Registry().emplace<Sprite>(objectId, Sprite{texture});
        Registry().emplace<Rotation>(objectId, Rotation{0});
        auto& rigidBody = Registry().emplace<RigidBody>(objectId);
        physicsInfo = {PhysicsTag::BULLET, objectId, &context.physics};

        b2CircleShape shape;
        shape.m_radius = 0.1f;
        auto createdCollider = Context().colliders.CreateDynamicBody(
            &shape, {startPosX, startPosY}, physicsInfo, reinterpret_cast<uintptr_t>(&params));
        try { collider = std::make_unique<Collider>(std::move(createdCollider)); }
        catch (...)
        {
            createdCollider.GetBody()->GetWorld()->DestroyBody(createdCollider.GetBody());
            throw;
        }
        collider->SetContinuous(true);
        collider->SetVelocity(b2Vec2(startVelX * params.maxSpeed, startVelY * params.maxSpeed));
        rigidBody.body = collider->GetBody();
        rigidBody.body->GetFixtureList()->SetRestitution(params.bounciness);
        rigidBody.body->SetGravityScale(params.gravityScale);
        collisionSubscription = Context().contacts.AddEvent(
            GetHandle(), CollisionType::BEGIN,
            std::bind(&Projectile::onCollision, this, std::placeholders::_1));
        if (camera) camera->ChangeTarget(GetHandle());
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}
