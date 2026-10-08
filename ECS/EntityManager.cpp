#include "ECS/EntityManager.h"

EntityManager::EntityManager()
{
	for ( EntityId e = 0u; e < MAX_ENTITIES; e++ )
		availableEntities.emplace( e );
}

EntityId EntityManager::CreateEntity()
{
	EntityId newEntity = availableEntities.front();
	LOG( "Created entity " + std::to_string( newEntity ) );
	availableEntities.pop();
	return newEntity;
}

void EntityManager::DestroyEntity( const EntityId ent )
{
	LOG( "Destroyed entity " + std::to_string( ent ) );
	availableEntities.push( ent );
}

Signature EntityManager::AddToSignature( const EntityId ent, const ComponentType type )
{
	return signatures[ent].set( type, true );
}

Signature EntityManager::DeleteFromSignature( const EntityId ent, const ComponentType type )
{
	return signatures[ent].set( type, false );
}

void EntityManager::SetSignature( const EntityId ent, const Signature signature )
{
	signatures[ent] = signature;
}

Signature EntityManager::GetSignature( const EntityId ent )
{
	return signatures[ent];
}

uint16_t EntityManager::GetAmountOfAvailableEntities() const
{
	return static_cast<uint16_t>(availableEntities.size());
}
