#pragma once
#include "Core/Audio/Music.h"
#include "Core/Audio/Sound.h"
#include "Core/SDLHandles.h"
#include <filesystem>
#include <unordered_map>

// Returned assets are borrowed until destruction. No eviction or hot reload.
class ResourceManager
{
public:
    // Null renderer supports headless contexts; texture loading requires a renderer.
    ResourceManager(SDL_Renderer* renderer, std::filesystem::path assetRoot);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    SDL_Texture* GetTexture(const std::filesystem::path& path);
    Sound& GetSound(const std::filesystem::path& path);
    Music& GetMusic(const std::filesystem::path& path);
    SDL_Renderer* Renderer() const noexcept { return renderer; }
    const std::filesystem::path& AssetRoot() const noexcept { return assetRoot; }

private:
    std::filesystem::path Resolve(const std::filesystem::path& path) const;
    SDL_Renderer* renderer;
    const std::filesystem::path assetRoot;
    std::unordered_map<std::filesystem::path, Sdl::TexturePtr> textures;
    std::unordered_map<std::filesystem::path, std::unique_ptr<Sound>> sounds;
    std::unordered_map<std::filesystem::path, std::unique_ptr<Music>> music;
};
