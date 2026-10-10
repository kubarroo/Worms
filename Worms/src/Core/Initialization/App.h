#pragma once
#include <SDL2/SDL.h> /* macOS- and GNU/Linux-specific */
#include <imgui.h>
#include <string>
#include "Core/SDLHandles.h"

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
    bool ShouldRenderColliders() const { return toggleColliders; }

private:
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
