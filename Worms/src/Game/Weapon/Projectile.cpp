#include <box2d/b2_body.h>
#include <box2d/b2_body.h>
#include <box2d/b2_circle_shape.h>
#include <box2d/b2_fixture.h>
#include <box2d/b2_world.h>
#include <SDL_image.h>
#include "Core/ParticleSystem.h"
#include "Game/GameScene.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Weapon/Projectile.h"

Projectile::Projectile( float posX, float posY, float vX, float vY ) : startPosX( posX ), startPosY( posY ), startVelX( vX ), startVelY( vY )
{
}

void Projectile::Update()
{
	if ( !HasEntity() || !collider ) return;
	auto& pos = world->GetComponent<Position>( objectId );
	if ( createSensor ||
		 pos.y < -15.f ||
		 timer.Measure() > params.explosionOffset && params.explosionOffset != 0 )
	{
		createSensor = false;
		if ( explosionSound ) explosionSound->Play();
        Scene().QueueAdd(std::make_unique<ParticleSystem>("particle.png", params.explosionRadius * 3.f, pos.x, pos.y, 100));

		world->GetComponent<RigidBody>( objectId ).body->SetAwake( true );
		sensorInfo.id = objectId;
		sensorInfo.tag = PhysicsTag::DESTRUCTION_FIELD;
		b2CircleShape shape;
		shape.m_radius = params.explosionRadius;
		fixture = ColliderFactory::Get().CreateTriggerFixture( collider->GetBody(), &shape, sensorInfo );
        Scene().RequestDestroy(*this);
	}
}

void Projectile::CleanUp()
{
    ContactManager::Get().RemoveEvent(collisionSubscription);
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
    ContactManager::Get().RemoveEvent(collisionSubscription);
    collisionSubscription = 0;
	if ( collisionSound ) collisionSound->Play();
	if ( params.explosionOffset == 0 )
		createSensor = true;

}

void Projectile::Initialise( SDL_Renderer* newRenderer, World* newWorld )
{
    GameObject::Initialise(newRenderer, newWorld);
    try
    {
        timer.Reset();
        world->AddComponent<Position>(objectId, {startPosX, startPosY});
        world->AddComponent<Sprite>(objectId, {texture});
        world->AddComponent<Rotation>(objectId, {0});
        auto& rigidBody = world->AddComponent<RigidBody>(objectId);
        physicsInfo.id = objectId;
        physicsInfo.tag = PhysicsTag::BULLET;

        b2CircleShape shape;
        shape.m_radius = 0.1f;
        auto createdCollider = ColliderFactory::Get().CreateDynamicBody(
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
        collisionSubscription = ContactManager::Get().AddEvent(
            objectId, CollisionType::BEGIN,
            std::bind(&Projectile::onCollision, this, std::placeholders::_1));
        if (camera) camera->ChangeTarget(objectId);
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}
