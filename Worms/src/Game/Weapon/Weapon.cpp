#include <algorithm>
#include "Core/Input.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Tags.h"
#include "Game/Weapon/Weapon.h"
#include "SDL_image.h"

Weapon::Weapon( Camera& camera ) : camera( camera )
{
}

void Weapon::Initialise(const SceneContext& context)
{
    GameObject::Initialise(context);
    try
	{
		world->AddComponent<Position>( objectId, { 0, 0 } );
		world->AddComponent<Rotation>( objectId, { 0 } );
		world->AddComponent<Sprite>( objectId );
		powerBar.reset( IMG_LoadTexture( renderer, "powerBar.png" ) );
		SDL_CHECK( powerBar.get() );
		canShoot = true;
	}
	catch (...)
	{
		CleanUp();
		throw;
	}
}

void Weapon::CleanUp()
{
    ClearParent();
    Deactivate();
    projTexture = nullptr;
    explosionSound = collisionSound = shootingSound = nullptr;
    GameObject::CleanUp();
    powerBar.reset();
}

std::optional<Position> Weapon::GetParentPosition()
{
	if (!HasEntity() || !parentId) return {};
	if (!world->IsAlive(*parentId))
	{
		ClearParent();
		return {};
	}
	auto parentPosition = world->TryGetComponent<Position>(parentId->id);
	if ( !parentPosition )
	{
		ClearParent();
		return {};
	}
	return parentPosition->get();
}

void Weapon::Update()
{
	auto parentPosition = GetParentPosition();
	if (!parentPosition || !canShoot) return;

	auto& pos = world->GetComponent<Position>( objectId );
	auto& rot = world->GetComponent<Rotation>( objectId );
	pos = *parentPosition;

	rot.degree += Input::Get().Vertical() * static_cast<float>(Time::deltaTime) * 100.f;

	pos.x += 0.1f * cosf( rot.degree * static_cast<float>(M_PI) / 180 );
	pos.y += 0.1f * sinf( rot.degree * static_cast<float>(M_PI) / 180 );

	if ( Input::Get().UseAction() )
	{
		if ( force < 1 )
			force += static_cast<float>(Time::deltaTime);
	}
	else
	{
		if ( force )
		{
			canShoot = false;
			if ( shootingSound ) shootingSound->Play();
            auto projectile = std::make_unique<Projectile>( pos.x + 0.5f * cosf( rot.degree * static_cast<float>(M_PI) / 180 ),
				pos.y + 0.5f * sinf( rot.degree * static_cast<float>(M_PI) / 180 ),
				force * cosf( rot.degree * static_cast<float>(M_PI) / 180 ),
                force * sinf( rot.degree * static_cast<float>(M_PI) / 180 ) );
            auto* proc = projectile.get();
			proc->SetGravityScale( weaponParams.gravityScale );
			proc->SetMaxSpeed( weaponParams.maxSpeed );
			proc->SetBaseDamage( weaponParams.baseDamage );
			proc->SetExplosionOffset( weaponParams.explosionOffset );
			proc->SetTexture( projTexture );
			proc->SetExplosionRadius( weaponParams.explosionRadius );
			proc->SetCollisionSound( collisionSound );
			proc->SetExplosionSound( explosionSound );
			proc->SetCamera( &camera );
			proc->SetBounciness( weaponParams.bounciness );
            Context().objects.QueueAdd(std::move(projectile));
        }
		force = 0;
	}
}

void Weapon::Render()
{
	if (!GetParentPosition() || !powerBar) return;
	auto& pos = world->GetComponent<Position>( objectId );
	auto& rot = world->GetComponent<Rotation>( objectId );
	SDL_Point size;
	SDL_QueryTexture( powerBar.get(), NULL, NULL, &size.x, &size.y );

	SDL_Rect slice(
		0,
		0,
		static_cast<int>(size.x * force),
		size.y );

	SDL_Rect renderQuad(
		400 + static_cast<int>((pos.x - camera.X()) * 100.0),
		300 - static_cast<int>((pos.y - camera.Y()) * 100.0) - size.y / 2,
		slice.w,
		slice.h );

	SDL_Point centre( 0, size.y / 2 );


	SDL_RenderCopyEx( renderer, powerBar.get(), &slice, &renderQuad, -rot.degree, &centre, SDL_FLIP_NONE );
}
