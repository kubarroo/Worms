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
    // Native handles are borrowed; null handles disable the corresponding asset loading.
    ResourceManager(SDL_Renderer* renderer, std::filesystem::path assetRoot,
                    MIX_Mixer* mixer = nullptr);
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) = delete;
    ResourceManager& operator=(ResourceManager&&) = delete;

    // Shared read-only asset: callers must not update pixels or persistent texture state
    // (color/alpha modulation, blend/scale mode). Mutable instances need private owners.
    SDL_Texture* GetTexture(const std::filesystem::path& path);
    Sound& GetSound(const std::filesystem::path& path);
    Music& GetMusic(const std::filesystem::path& path);
    SDL_Renderer* Renderer() const noexcept { return renderer; }
    MIX_Mixer* Mixer() const noexcept { return mixer; }
    const std::filesystem::path& AssetRoot() const noexcept { return assetRoot; }

private:
    std::filesystem::path Resolve(const std::filesystem::path& path) const;
    SDL_Renderer* renderer;
    MIX_Mixer* mixer;
    const std::filesystem::path assetRoot;
    std::unordered_map<std::filesystem::path, Sdl::TexturePtr> textures;
    std::unordered_map<std::filesystem::path, std::unique_ptr<Sound>> sounds;
    std::unordered_map<std::filesystem::path, std::unique_ptr<Music>> music;
};
