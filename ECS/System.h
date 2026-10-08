#pragma once
#include <set>
#include "ECS/ComponentManager.h"
#include "ECS/ECS_Types.h"

class System
{
public:
	System( ComponentManager& componentManager ) : componentManager( componentManager ) {}
	virtual void Update() {};
	virtual void Render() {};
	inline Signature GetSystemSignature() const { return systemSignature; }
	bool CheckIfSubscribed( const EntityId ent ) const { return subscribed.find( ent ) != subscribed.end(); }
	void Subscribe( const EntityId ent ) { subscribed.insert( ent ); }
	void Unsubscribe( const EntityId ent ) { subscribed.erase( ent ); }
	uint16_t AmountOfEnttiesInSystem() const { return static_cast<uint16_t>(subscribed.size()); }

protected:
	Signature systemSignature;
	std::set<EntityId> subscribed;
	ComponentManager& componentManager;
};
