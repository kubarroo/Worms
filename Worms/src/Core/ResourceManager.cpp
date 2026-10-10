#include "ResourceManager.h"
#include <SDL_image.h>
#include <stdexcept>
#include <utility>

ResourceManager::ResourceManager(SDL_Renderer* renderer, std::filesystem::path assetRoot)
    : renderer(renderer), assetRoot(std::filesystem::absolute(assetRoot).lexically_normal())
{
}

std::filesystem::path ResourceManager::Resolve(const std::filesystem::path& path) const
{
    if (path.empty())
        throw std::invalid_argument("Asset path must not be empty");
    return (assetRoot / path).lexically_normal();
}

SDL_Texture* ResourceManager::GetTexture(const std::filesystem::path& path)
{
    if (!renderer)
        throw std::logic_error("Texture loading requires a live renderer");
    const auto key = Resolve(path);
    if (const auto found = textures.find(key); found != textures.end())
        return found->second.get();
    const auto filename = key.string();
    Sdl::TexturePtr texture(IMG_LoadTexture(renderer, filename.c_str()));
    if (!texture)
    {
        const auto message = "Could not load texture '" + filename + "': " + IMG_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
    return textures.emplace(key, std::move(texture)).first->second.get();
}

Sound& ResourceManager::GetSound(const std::filesystem::path& path)
{
    if (!Mix_QuerySpec(nullptr, nullptr, nullptr))
        throw std::logic_error("Sound loading requires an open audio device");
    const auto key = Resolve(path);
    if (const auto found = sounds.find(key); found != sounds.end())
        return *found->second;
    auto sound = std::make_unique<Sound>(key.string());
    return *sounds.emplace(key, std::move(sound)).first->second;
}

Music& ResourceManager::GetMusic(const std::filesystem::path& path)
{
    if (!Mix_QuerySpec(nullptr, nullptr, nullptr))
        throw std::logic_error("Music loading requires an open audio device");
    const auto key = Resolve(path);
    if (const auto found = music.find(key); found != music.end())
        return *found->second;
    auto track = std::make_unique<Music>(key.string());
    return *music.emplace(key, std::move(track)).first->second;
}
