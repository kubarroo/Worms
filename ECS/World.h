#pragma once
#include <optional>
#include <SDL2/SDL.h>
#include "ECS/ComponentManager.h"
#include "ECS/EntityManager.h"
#include "ECS/SystemManager.h"

class World
{
public:
	World( SDL_Renderer* renderer );
	World( const World& ) = delete;
	World( World&& ) = delete;

	EntityId CreateEntity();
	void DestroyEntity( const EntityId ent );

	EntityId CopyEntity( const EntityId toCopy );

	template<typename Component>
	void RegisterComponent();
	template<typename System, typename... Args>
	void RegisterSystem( Args&&... params );

	template<typename Component>
	Component& AddComponent( const EntityId ent );
	template<typename Component>
	Component& AddComponent( const EntityId ent, const Component&& component );
	template<typename Component>
	Component& GetComponent( const EntityId ent );
	template<typename Component>
	std::optional<std::reference_wrapper<Component>> TryGetComponent( const EntityId ent );
	template<typename Component>
	Component RemoveComponent( const EntityId ent );


	void Update();
	void Render();

private:
	SDL_Renderer* renderer;
private:
	EntityManager entManager{};
	ComponentManager comManager{};
	SystemManager sysManager{ comManager };
};

template<typename Component>
inline void World::RegisterComponent()
{
	LOG( "Registered " + (std::string)typeid(Component).name() + " Component" );
	comManager.RegisterComponent<Component>();
}

template<typename System, typename... Args>
inline void World::RegisterSystem( Args&&... params )
{
	LOG( "Registered " + (std::string)typeid(System).name() + " System" );
	sysManager.RegisterSystem<System>( std::forward<Args>( params )... );
}

template<typename Component>
Component& World::AddComponent( const EntityId ent )
{
	Signature newSign = entManager.AddToSignature( ent, comManager.GetComponentId<Component>() );
	sysManager.OnSignatureChange( ent, newSign );
	return comManager.AddComponent<Component>( ent );
}

template<typename Component>
inline Component& World::AddComponent( const EntityId ent, const Component&& component )
{
	Signature newSign = entManager.AddToSignature( ent, comManager.GetComponentId<Component>() );
	sysManager.OnSignatureChange( ent, newSign );
	return comManager.AddComponent<Component>( ent, std::move( component ) );
}

template<typename Component>
Component& World::GetComponent( const EntityId ent )
{
	return comManager.GetComponent<Component>( ent );
}

template<typename Component>
std::optional<std::reference_wrapper<Component>> World::TryGetComponent( const EntityId ent )
{
	return comManager.TryGetComponent<Component>( ent );
}

template<typename Component>
Component World::RemoveComponent( const EntityId ent )
{
	Signature newSign = entManager.DeleteFromSignature( ent, comManager.GetComponentId<Component>() );
	sysManager.OnSignatureChange( ent, newSign );
	return comManager.RemoveComponent<Component>( ent );
}
