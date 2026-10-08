#include "Game/Map/Map.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Utils.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Game/Components.h"
#include "SDL2/SDL_surface.h"
#include "box2d/b2_chain_shape.h"
#include "box2d/b2_fixture.h"
#include "box2d/b2_polygon_shape.h"
#include "box2d/b2_world.h"
#include <memory>


Map::Map(b2World* physicsWorld) : physicsWorld(physicsWorld) {}

void Map::Initialise(SDL_Renderer* renderer, World* world)
{
    GameObject::Initialise(renderer, world);

    physicsInfo.id = objectId;
    physicsInfo.tag = PhysicsTag::MAP;
    world->AddComponent<Position>(objectId, {1.5f, -2.f});

    physTex = IMG_LoadPhysicTexture(renderer, "map.png");
    ContactManager::Get().AddEvent(objectId, CollisionType::BEGIN,
                                   std::bind(&Map::DestroyMap, this, std::placeholders::_1));
    if (physTex.has_value())
    {
        auto& rb = world->AddComponent<RigidBody>(objectId);
        auto texture = SDL_CreateTextureFromSurface(renderer, physTex.value().surface);
        SDL_QueryTexture(texture, NULL, NULL, &mapSize.x, &mapSize.y);
        CreateNewColliders();

        world->AddComponent<Sprite>(objectId, {texture});
    }
}

void Map::Update()
{
    if (!destroyed || physicsWorld->IsLocked())
        return;
    destroyed = false;

    Position mapPos = world->GetComponent<Position>(objectId);
    DestroyMapAtLocalPoint(GlobalToLocalPos(mapPos));
    CreateNewColliders();

    auto texture = SDL_CreateTextureFromSurface(renderer, physTex.value().surface);
    SDL_DestroyTexture(world->GetComponent<Sprite>(objectId).texture);
    world->GetComponent<Sprite>(objectId).texture = texture;
}

void Map::CleanUp()
{
    SDL_DestroyTexture(world->GetComponent<Sprite>(objectId).texture);
}

SDL_Point Map::GlobalToLocalPos(const Position& mapPos)
{
    b2Vec2 localSpace = {bulltetPos.x - mapPos.x, -bulltetPos.y + mapPos.y};
    SDL_Point point = {static_cast<int>(localSpace.x * 100.f),
                       static_cast<int>(localSpace.y * 100.f)};
    point.x += mapSize.x / 2;
    point.y += mapSize.y / 2;
    return point;
}

void Map::DestroyMapAtLocalPoint(SDL_Point point)
{
    auto surf = physTex.value().surface;
    for (int y = 0; y < surf->h; y++)
    {
        for (int x = 0; x < surf->w; x++)
        {
            float dist = Distance(x, y, point.x, point.y);
            Uint8* pixel = (Uint8*)surf->pixels;
            pixel += (y * surf->pitch) + (x * sizeof(Uint32));

            if (dist < destructionRadius * 100)
                *((Uint32*)pixel) &= 0x00000000;
        }
    }
}

void Map::DestroyMap(b2Contact* contact)
{
    auto entId = GetEntityWithTag(contact, PhysicsTag::DESTRUCTION_FIELD);
    if (!entId.has_value())
        return;

    bulltetPos = world->GetComponent<Position>(entId.value());
    auto contactBody = GetObjectWithTag(contact, PhysicsTag::DESTRUCTION_FIELD);
    destructionRadius =
        reinterpret_cast<Parameters*>(contactBody.value()->GetUserData().pointer)->explosionRadius;
    if (destroyed)
        return;

    destroyed = true;
}

float Map::Distance(const float x1, const float y1, const float x2, const float y2)
{
    return std::sqrt(std::pow(x2 - x1, 2) + std::pow(y2 - y1, 2));
}

void Map::CreateNewColliders()
{

    std::vector<std::vector<b2Vec2>> physPoints = CreateContour();
    SimplifyContour(physPoints);

    physTex->points = physPoints;

    if (world->GetComponent<RigidBody>(objectId).body != NULL)
        physicsWorld->DestroyBody(world->GetComponent<RigidBody>(objectId).body);

    b2ChainShape shape;
    shape.CreateLoop(&physTex->points[0][0], physTex->points[0].size());
    auto& pos = world->GetComponent<Position>(objectId);
    auto collider = ColliderFactory::Get().CreateStaticBody(&shape, {pos.x, pos.y}, physicsInfo);
    GenerateFixturesForAllContours(collider);

    world->GetComponent<RigidBody>(objectId).body = collider.GetBody();
}

void Map::GenerateFixturesForAllContours(Collider& collider)
{
    b2ChainShape shape;
    for (int i = 1; i < physTex->points.size(); i++)
    {
        shape.CreateLoop(&physTex.value().points[i][0], physTex.value().points[i].size());
        ColliderFactory::Get().CreateStaticFixture(collider.GetBody(), &shape, physicsInfo);
        shape.Clear();
    }
}

std::vector<std::vector<b2Vec2>> Map::CreateContour()
{
    std::vector<std::vector<b2Vec2>> physPoints;
    auto surf = physTex->surface;
    auto shapes = MarchingSquares((Uint32*)surf->pixels, surf->w, surf->h, 64);
    for (int i = 0; i < shapes.size(); i++)
    {
        physPoints.push_back({});
        for (int j = shapes[i].size() - 1; j >= 0; j--)
            physPoints[i].emplace_back(float(shapes[i][j].x / 100.f),
                                       float(-shapes[i][j].y / 100.f));
    }

    return std::move(physPoints);
}

void Map::SimplifyContour(std::vector<std::vector<b2Vec2>>& physPoints)
{
    for (auto& points : physPoints)
    {
        points = DouglasPeucker(points, 0.05);
        for (auto& point : points)
        {
            point.x -= mapSize.x / 200.f;
            point.y += mapSize.y / 200.f;
        }
    }
}
