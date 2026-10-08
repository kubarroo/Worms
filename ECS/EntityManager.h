#pragma once
#include <array>
#include <bitset>
#include <queue>
#include "ECS/ECS_Types.h"

class EntityManager
{
public:
	EntityManager();

	EntityId CreateEntity();
	void DestroyEntity( const EntityId ent );

	Signature AddToSignature( const EntityId ent, const ComponentType type );
	Signature DeleteFromSignature( const EntityId ent, const ComponentType type );
	void SetSignature( const EntityId ent, const Signature signature );
	Signature GetSignature( const EntityId ent );

	uint16_t GetAmountOfAvailableEntities() const;

private:
	std::queue<EntityId> availableEntities;
	std::array<Signature, MAX_ENTITIES> signatures;

};

