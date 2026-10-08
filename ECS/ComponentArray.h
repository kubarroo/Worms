#pragma once
#include <array>
#include <optional>
#include <unordered_map>
#include "ECS/ECS_Types.h"

class IComponentArray
{
public:
	virtual ~IComponentArray() {}
	virtual void EraseData( const EntityId ent ) {}
	virtual void AddDataNoReturn( const EntityId ent ) {}
};

template<typename Data>
class ComponentArray : public IComponentArray
{
public:
	Data& AddData( const EntityId ent );
	Data& AddData( const EntityId ent, const Data&& data );
	void AddDataNoReturn( const EntityId ent );
	Data& GetData( const EntityId ent );
	std::optional<std::reference_wrapper<Data>> TryGetData( const EntityId ent );
	Data& GetDataAtIndex( const EntityId ent );
	void SetData( const EntityId ent, const Data& data );
	void EraseData( const EntityId ent );

	ComponentType GetType;
	virtual ~ComponentArray() {}
private:
	ComponentType typeId;

	std::array<Data, MAX_ENTITIES> components;
	std::unordered_map<EntityId, EntityId> entityToIndex;
	std::unordered_map<EntityId, EntityId> indexToEntity;

	uint16_t amountOfComponents = 0;
};

template<typename Data>
Data& ComponentArray<Data>::AddData( const EntityId ent )
{
	if ( amountOfComponents == MAX_ENTITIES )
		ECS_THROW( "Entities overflow" );

	entityToIndex[ent] = amountOfComponents;
	indexToEntity[amountOfComponents] = ent;

	return components[amountOfComponents++];
}

template<typename Data>
inline Data& ComponentArray<Data>::AddData( const EntityId ent, const Data&& data )
{
	if ( amountOfComponents == MAX_ENTITIES )
		ECS_THROW( "Entities overflow" );

	entityToIndex[ent] = amountOfComponents;
	indexToEntity[amountOfComponents] = ent;
	components[amountOfComponents] = data;
	amountOfComponents++;

	return components[amountOfComponents - 1];
}

template<typename Data>
void ComponentArray<Data>::AddDataNoReturn( const EntityId ent )
{
	AddData( ent );
}

template<typename Data>
Data& ComponentArray<Data>::GetData( const EntityId ent )
{
	auto it = entityToIndex.find( ent );
	if ( it == entityToIndex.end() )
		ECS_THROW( "You are trying to access component that doesn't exist" );
	EntityId index = it->second;
	return components[index];
}

template<typename Data>
std::optional<std::reference_wrapper<Data>>  ComponentArray<Data>::TryGetData( const EntityId ent )
{
	auto it = entityToIndex.find( ent );
	if ( it == entityToIndex.end() )
		return{};
	EntityId index = it->second;
	return components[index];
}

template<typename Data>
inline Data& ComponentArray<Data>::GetDataAtIndex( const EntityId index )
{
	if ( index >= amountOfComponents )
		ECS_THROW( "You are trying to access component by index that doesn't exist" );

	return components[index];
}

template<typename Data>
void ComponentArray<Data>::SetData( const EntityId ent, const Data& data )
{
	auto it = entityToIndex.find( ent );
	if ( it == entityToIndex.end() )
		ECS_THROW( "You are trying to copy component to component that doesn't exist" );
	EntityId index = it->second;

	components[index] = data;
}

template<typename Data>
void ComponentArray<Data>::EraseData( const EntityId ent )
{
	auto it = entityToIndex.find( ent );
	if ( it == entityToIndex.end() )
		ECS_THROW( "Component you are trying to erase doesn't exist" );

	const EntityId index = it->second;

	amountOfComponents--;

	components[index] = components[amountOfComponents];
	components[amountOfComponents] = Data{};

	entityToIndex[indexToEntity[amountOfComponents]] = index;
	entityToIndex.erase( ent );

	indexToEntity[index] = indexToEntity[amountOfComponents];
	indexToEntity.erase( amountOfComponents );
}
