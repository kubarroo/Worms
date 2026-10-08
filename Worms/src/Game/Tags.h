#pragma once
#include "ECS/ECS_Types.h"
#include <box2d/b2_contact.h>
#include <optional>


enum PhysicsTag
{
    NONE,
    BULLET,
    DESTRUCTION_FIELD,
    WORM,
    GROUNDED,
    MAP
};

struct PhysicsInfo
{
    PhysicsTag tag;
    EntityId id;
};

struct Parameters
{
    float explosionOffset;
    std::string texturePath;
    float explosionRadius;
    float baseDamage;
    float gravityScale;
    float maxSpeed;
    float bounciness;
};

std::optional<EntityId> GetEntityWithTag(b2Contact* contact, PhysicsTag tag);
std::optional<b2Body*> GetObjectWithTag(b2Contact* contact, PhysicsTag tag);
