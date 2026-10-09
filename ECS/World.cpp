#include "ECS/World.h"

World::World(SDL_Renderer* renderer) : renderer(renderer) {}

EntityId World::CreateEntity()
{
    return entManager.CreateEntity();
}

void World::DestroyEntity(const EntityId ent)
{
    if (!IsAlive(ent)) return;
    comManager.RemoveAllComponents(ent, entManager.GetSignature(ent));
    entManager.DestroyEntity(ent);
    sysManager.UnsubscribeEntity(ent);
}

EntityId World::CopyEntity(const EntityId toCopy)
{
    Signature toCopySign = entManager.GetSignature(toCopy);
    EntityId newId = entManager.CreateEntity();
    entManager.SetSignature(newId, toCopySign);
    comManager.AddComponents(newId, toCopySign);
    sysManager.OnSignatureChange(newId, toCopySign); // Maybe could be optimised
    return newId;
}

void World::Update()
{
    sysManager.Update();
}

void World::Render()
{
    sysManager.Render();
}
