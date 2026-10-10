#include <algorithm>
#include "Core/Input.h"
#include "Core/Audio/Audio.h"
#include "Core/Renderer2D.h"
#include "Core/ResourceManager.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Tags.h"
#include "Game/Weapon/Weapon.h"

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
		powerBar = context.resources.GetTexture("powerBar.png");
		canShoot = true;
        observedInputInterruption = context.input.InterruptionCount();
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
    powerBar = nullptr;
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
    if (!HasEntity()) return;
    const auto& input = Context().input;
    const auto interruption = input.InterruptionCount();
    // A skipped update must not lose cancellation of a previously charged shot.
    if (!input.Enabled() || input.Interrupted() || observedInputInterruption != interruption)
    {
        observedInputInterruption = interruption;
        force = 0;
        return;
    }
	auto parentPosition = GetParentPosition();
	if (!parentPosition || !canShoot) return;

	auto& pos = world->GetComponent<Position>( objectId );
	auto& rot = world->GetComponent<Rotation>( objectId );
	pos = *parentPosition;

	rot.degree += Context().input.Vertical() * static_cast<float>(Time::deltaTime) * 100.f;

	pos.x += 0.1f * cosf( rot.degree * static_cast<float>(M_PI) / 180 );
	pos.y += 0.1f * sinf( rot.degree * static_cast<float>(M_PI) / 180 );

	if ( Context().input.UseAction() )
	{
		if ( force < 1 )
			force += static_cast<float>(Time::deltaTime);
	}
	else
	{
		if ( force )
		{
			canShoot = false;
			if ( shootingSound ) Context().audio.Play(*shootingSound);
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
	auto size = Context().rendering.TextureSize(powerBar);

	RenderRect slice(
		0,
		0,
		static_cast<int>(size.x * force),
		size.y );

	RenderRect renderQuad(
		400 + static_cast<int>((pos.x - camera.X()) * 100.0),
		300 - static_cast<int>((pos.y - camera.Y()) * 100.0) - size.y / 2,
		slice.w,
		slice.h );

	RenderPoint centre( 0, size.y / 2 );


	Context().rendering.DrawSprite(powerBar, renderQuad, slice, -rot.degree, centre);
}
