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
    Map() = default;
    void Initialise(const SceneContext& context) override;
    void Update() override;
    void CleanUp() override;

    SDL_Point GlobalToLocalPos(const Position& mapPos);
    void DestroyMapAtLocalPoint(SDL_Point point);

private:
    friend struct MapTestAccess;
    void DestroyMap(b2Contact* contact);
    float Distance(const float x1, const float y1, const float x2, const float y2);

    void CreateNewColliders();

    void GenerateFixturesForAllContours(Collider& collider,
                                        const std::vector<std::vector<b2Vec2>>& contours);

    std::vector<std::vector<b2Vec2>> CreateContour();

    void SimplifyContour(std::vector<std::vector<b2Vec2>>& physPoints);

    bool destroyed = false;
    SubscriptionId destructionSubscription = 0;
    b2World* physicsWorld = nullptr;
    b2Body* mapBody = nullptr;

    Position bulltetPos;
    PhysicsInfo physicsInfo;
    // Private per-map pixels and texture; Sprite only borrows mapTexture.
    std::optional<PhysicTexture> physTex;
    Sdl::TexturePtr mapTexture;
    float destructionRadius = 0;

    SDL_Point mapSize{};
};
