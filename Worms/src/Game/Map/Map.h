#pragma once
#include "Core/GameObject.h"
#include "Core/Physics/Collider.h"
#include "Core/Utils.h"
#include "ECS/World.h"
#include "Game/Components.h"
#include "Game/Tags.h"
#include <SDL2/SDL.h>
#include <memory>


class Map : public GameObject
{
public:
    Map(b2World* physicsWorld);
    void Initialise(SDL_Renderer* renderer, World* world) override;
    void Update() override;
    void CleanUp() override;

    SDL_Point GlobalToLocalPos(const Position& mapPos);
    void DestroyMapAtLocalPoint(SDL_Point point);

private:
    void DestroyMap(b2Contact* contact);
    float Distance(const float x1, const float y1, const float x2, const float y2);

    void CreateNewColliders();

    void GenerateFixturesForAllContours(Collider& collider);

    std::vector<std::vector<b2Vec2>> CreateContour();

    void SimplifyContour(std::vector<std::vector<b2Vec2>>& physPoints);

    bool destroyed = false;
    b2World* physicsWorld;

    Position bulltetPos;
    PhysicsInfo physicsInfo;
    std::optional<PhysicTexture> physTex;
    float destructionRadius;

    SDL_Point mapSize{};
};
