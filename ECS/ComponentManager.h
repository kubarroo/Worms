#pragma once

#include <array>
#include <optional>
#include <sstream>
#include "ComponentArray.h"


class ComponentManager
{
public:
	template<typename T>
	void RegisterComponent();

	template<typename T>
	ComponentArray<T>& GetComponentArray();

	template<typename T>
	T& GetComponent( const EntityId ent );

	template<typename T>
	std::optional<std::reference_wrapper<T>> TryGetComponent( const EntityId ent );

	template<typename T>
	uint8_t GetComponentId() const;

	template<typename T>
	T& AddComponent( const EntityId ent );
	template<typename T>
	T& AddComponent( const EntityId ent, const T&& );
	void AddComponents( const EntityId ent, const Signature sign );

	template<typename T>
	void RemoveComponent( const EntityId ent );

	void RemoveAllComponents( const EntityId ent, const Signature sign );

	std::vector<std::string> GetRegisteredComponents() const;

	~ComponentManager();
private:
	std::array<IComponentArray*, MAX_COMPONENTS> components;
	std::unordered_map<const char*, uint8_t> typeToComponentId;
	uint8_t registeredComponents = 0u;

};

template<typename T>
void ComponentManager::RegisterComponent()
{
	if ( registeredComponents == MAX_COMPONENTS )
		ECS_THROW( "ComponentArray overflow" );
	if ( typeToComponentId.find( typeid(T).name() ) != typeToComponentId.end() )
		ECS_THROW( "Component already refistered" );

	components[registeredComponents] = new ComponentArray<T>();
	typeToComponentId.insert( { typeid(T).name(), registeredComponents++ } );
}

template<typename T>
ComponentArray<T>& ComponentManager::GetComponentArray()
{
	auto it = typeToComponentId.find( typeid(T).name() );
	if ( it == typeToComponentId.end() )
		ECS_THROW( "You are trying to access unregistered component array" );
	EntityId index = it->second;
	return *static_cast<ComponentArray<T>*> (components[index]);
}

template<typename T>
T& ComponentManager::GetComponent( const EntityId ent )
{
	return GetComponentArray<T>().GetData( ent );
}

template<typename T>
std::optional<std::reference_wrapper<T>> ComponentManager::TryGetComponent( const EntityId ent )
{
	return GetComponentArray<T>().TryGetData( ent );
}

template<typename T>
uint8_t ComponentManager::GetComponentId() const
{
	auto it = typeToComponentId.find( typeid(T).name() );
	if ( it == typeToComponentId.end() )
		ECS_THROW( "You are trying to access unregistered component array" );
	return it->second;
}

template<typename T>
T& ComponentManager::AddComponent( const EntityId ent )
{
	return GetComponentArray<T>().AddData( ent );
}

template<typename T>
T& ComponentManager::AddComponent( const EntityId ent, const T&& data )
{
	return GetComponentArray<T>().AddData( ent, std::move( data ) );
}

template<typename T>
void ComponentManager::RemoveComponent( const EntityId ent )
{
	GetComponentArray<T>().EraseData( ent );
}



