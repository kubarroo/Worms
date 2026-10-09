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
#include <algorithm>

Map::Map(b2World* physicsWorld) : physicsWorld(physicsWorld) {}

void Map::Initialise(SDL_Renderer* renderer, World* world)
{
    if (!physicsWorld) throw std::invalid_argument("Map requires a physics world");
    GameObject::Initialise(renderer, world);
    try
    {
        physicsInfo.id = objectId;
        physicsInfo.tag = PhysicsTag::MAP;
        world->AddComponent<Position>(objectId, {1.5f, -2.f});
        physTex = IMG_LoadPhysicTexture(renderer, "map.png");
        SDL_CHECK((physTex ? physTex->surface.get() : nullptr));
        destructionSubscription = ContactManager::Get().AddEvent(objectId, CollisionType::BEGIN,
                                       std::bind(&Map::DestroyMap, this, std::placeholders::_1));
        world->AddComponent<RigidBody>(objectId);
        mapTexture.reset(SDL_CreateTextureFromSurface(renderer, physTex->surface.get()));
        SDL_CHECK(mapTexture.get());
        SDL_CALL(SDL_QueryTexture(mapTexture.get(), nullptr, nullptr, &mapSize.x, &mapSize.y));
        world->AddComponent<Sprite>(objectId, {mapTexture.get()});
        CreateNewColliders();
    }
    catch (...)
    {
        CleanUp();
        throw;
    }
}

void Map::Update()
{
    if (!HasEntity() || !physTex || !destroyed || physicsWorld->IsLocked())
        return;

    Position mapPos = world->GetComponent<Position>(objectId);
    DestroyMapAtLocalPoint(GlobalToLocalPos(mapPos));
    Sdl::TexturePtr texture(SDL_CreateTextureFromSurface(renderer, physTex->surface.get()));
    SDL_CHECK(texture.get());
    auto& sprite = world->GetComponent<Sprite>(objectId);
    CreateNewColliders();

    sprite.texture = texture.get();
    mapTexture = std::move(texture);
    destroyed = false;
}

void Map::CleanUp()
{
    if (!HasEntity())
    {
        return;
    }
    ContactManager::Get().RemoveEvent(destructionSubscription);
    destructionSubscription = 0;
    if (mapBody)
    {
        physicsWorld->DestroyBody(mapBody);
        mapBody = nullptr;
    }
    GameObject::CleanUp();
    mapTexture.reset();
    physTex.reset();
    destroyed = false;
    destructionRadius = 0;
    mapSize = {};
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
    if (!HasEntity() || !physTex) return;
    auto* surf = physTex->surface.get();
    Sdl::SurfaceLock lock(surf);
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

    std::erase_if(physPoints, [](const auto& points)
    {
        if (points.size() < 3) return true;
        for (std::size_t i = 0; i < points.size(); ++i)
            if (b2DistanceSquared(points[i], points[(i + 1) % points.size()]) <
                b2_linearSlop * b2_linearSlop)
                return true;
        return false;
    });

    auto& body = world->GetComponent<RigidBody>(objectId).body;
    b2Body* replacement = nullptr;
    if (!physPoints.empty())
    {
        b2ChainShape shape;
        shape.CreateLoop(physPoints.front().data(), static_cast<int32>(physPoints.front().size()));
        const auto& pos = world->GetComponent<Position>(objectId);
        auto collider = ColliderFactory::Get().CreateStaticBody(&shape, {pos.x, pos.y}, physicsInfo);
        replacement = collider.GetBody();
        try
        {
            GenerateFixturesForAllContours(collider, physPoints);
        }
        catch (...)
        {
            physicsWorld->DestroyBody(replacement);
            throw;
        }
    }

    if (body)
    {
        physicsWorld->DestroyBody(body);
        body = nullptr;
    }
    body = replacement;
    mapBody = replacement;
    physTex->points = std::move(physPoints);
}

void Map::GenerateFixturesForAllContours(Collider& collider,
                                       const std::vector<std::vector<b2Vec2>>& contours)
{
    b2ChainShape shape;
    for (std::size_t i = 1; i < contours.size(); i++)
    {
        shape.CreateLoop(contours[i].data(), static_cast<int32>(contours[i].size()));
        ColliderFactory::Get().CreateStaticFixture(collider.GetBody(), &shape, physicsInfo);
        shape.Clear();
    }
}

std::vector<std::vector<b2Vec2>> Map::CreateContour()
{
    std::vector<std::vector<b2Vec2>> physPoints;
    auto* surf = physTex->surface.get();
    Sdl::SurfaceLock lock(surf);
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
        if (points.size() < 3)
            continue;
        points = DouglasPeucker(points, 0.05);
        for (auto& point : points)
        {
            point.x -= mapSize.x / 200.f;
            point.y += mapSize.y / 200.f;
        }
    }
}
