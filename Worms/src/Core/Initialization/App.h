#pragma once
#include <SDL2/SDL.h> /* macOS- and GNU/Linux-specific */
#include <imgui.h>
#include <string>
#include "Core/SDLHandles.h"
#include <filesystem>
#include "Core/Input.h"
#include "Core/SdlInputAdapter.h"
#include "Core/Renderer2D.h"
#include "Core/Audio/Audio.h"

class ResourceManager;

class App
{
public:
    App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;
    App(App&&) = delete;
    App& operator=(App&&) = delete;
    virtual void InitWindow(const std::string& title, const int width, const int height);
    virtual void Update();
    virtual void HandleEvents();
    virtual void Render();
    void PostRender();
    void PreRender();
    virtual void Clean() noexcept;
    // Configuration for the next platform initialization; never changes a live cache.
    void SetAssetRoot(std::filesystem::path root);

    inline bool IsRunning() const
    {
        return isRunning;
    }
    virtual ~App();

protected:
    // Declaration order keeps the renderer's destruction before its window.
    Sdl::WindowPtr window;
    Sdl::RendererPtr renderer;
    bool audioOpened = false;
    void StopAudioPlayback() noexcept;
    ResourceManager& Resources() const;
    Renderer2D& Graphics() const;
    Audio& AudioOutput() { return audio; }
    Input& Inputs() { return input; }
    bool ShouldRenderColliders() const { return toggleColliders; }

private:
    std::filesystem::path assetRoot = ".";
    std::unique_ptr<ResourceManager> resources;
    std::unique_ptr<Renderer2D> graphics;
    Audio audio;
    Input input;
    SdlInputAdapter inputAdapter;
    void InitSDL(const std::string& title, const int width, const int height);
    void InitImGui();
    ImGuiIO* io = nullptr;
    ImGuiContext* imguiContext = nullptr;
    bool sdlInitialized = false;
    bool imguiPlatformInitialized = false;
    bool imguiRendererInitialized = false;

    bool toggleColliders = false;
    bool isRunning = false;
};
