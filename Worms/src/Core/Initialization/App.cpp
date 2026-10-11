#include "Core/Initialization/App.h"
#include "Core/Input.h"
#include "Core/ResourceManager.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl3.h"
#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <imgui_impl_sdlrenderer3.h>
#include <stdexcept>


App::App() : inputAdapter(input) {}

void App::SetAssetRoot(std::filesystem::path root)
{
    if (sdlInitialized || resources)
        throw std::logic_error("Cannot change asset root while the platform is initialized");
    if (root.empty())
        throw std::invalid_argument("Asset root must not be empty");
    assetRoot = std::filesystem::absolute(root).lexically_normal();
}

ResourceManager& App::Resources() const
{
    if (!resources)
        throw std::logic_error("Resource manager is unavailable");
    return *resources;
}

Renderer2D& App::Graphics() const
{
    if (!graphics) throw std::logic_error("Renderer adapter is unavailable");
    return *graphics;
}

App::~App()
{
    App::Clean();
}

void App::InitWindow(const std::string& title, const int width, const int height)
{
    if (sdlInitialized || imguiContext)
        throw std::logic_error("App is already initialized");
    try
    {
        InitSDL(title, width, height);
        logicalWidth = width;
        logicalHeight = height;
        graphics = std::make_unique<Renderer2D>(renderer.get());
        graphics->SetLogicalSize(width, height);
        inputAdapter.Reset();
        resources = std::make_unique<ResourceManager>(renderer.get(), assetRoot, audio.Native());
        InitImGui();
        isRunning = true;
    }
    catch (...)
    {
        App::Clean();
        throw;
    }
}

void App::InitSDL(const std::string& title, const int width, const int height)
{
    // Initialize audio first: a failing audio driver must not roll back a live
    // video backend while its device-discovery thread is still starting.
    try
    {
        SDL_CALL(SDL_Init(SDL_INIT_AUDIO));
    }
    catch (...)
    {
        SDL_Quit();
        throw;
    }
    sdlInitialized = true;
    SDL_CALL(SDL_InitSubSystem(SDL_INIT_VIDEO));

    /* Creates a SDL window */
    window.reset(SDL_CreateWindow(title.c_str(), width, height, 0));
    SDL_CHECK(window.get());

    renderer.reset(SDL_CreateRenderer(window.get(), nullptr));
    SDL_CHECK(renderer.get());
    SDL_CALL(SDL_SetDefaultTextureScaleMode(renderer.get(), SDL_SCALEMODE_NEAREST));

    // SDL may simulate VSync when the renderer has no native support.
    vsyncEnabled = SDL_SetRenderVSync(renderer.get(), 1);
    if (!vsyncEnabled)
        Terminal::Get().Log(std::string("Could not enable VSync; using a 60 FPS limit: ") +
                            SDL_GetError(), LogLevel::WARNING);
    audio.Open();
    audioOpened = true;
}

void App::InitImGui()
{
    IMGUI_CHECKVERSION();
    imguiContext = ImGui::CreateContext();
    if (!imguiContext)
        throw std::runtime_error("Could not create ImGui context");
    io = &ImGui::GetIO();
    (void)*io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    imguiPlatformInitialized = ImGui_ImplSDL3_InitForSDLRenderer(window.get(), renderer.get());
    if (!imguiPlatformInitialized)
        throw std::runtime_error("Could not initialize ImGui SDL3 backend");
    imguiRendererInitialized = ImGui_ImplSDLRenderer3_Init(renderer.get());
    if (!imguiRendererInitialized)
        throw std::runtime_error("Could not initialize ImGui renderer backend");
}

void App::Update()
{
    Terminal::Get().Update();
}

void App::HandleEvents()
{
    if (!io || !sdlInitialized || !imguiPlatformInitialized)
        return;
    inputAdapter.BeginFrame(io->WantCaptureKeyboard);
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        ImGui_ImplSDL3_ProcessEvent(&ev);
        inputAdapter.ProcessEvent(ev, io->WantCaptureKeyboard);
        switch (ev.type)
        {
        case SDL_EVENT_QUIT:
            isRunning = false;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (ev.window.windowID == SDL_GetWindowID(window.get()))
                isRunning = false;
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            if (ev.window.windowID == SDL_GetWindowID(window.get()))
                minimized = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            if (ev.window.windowID == SDL_GetWindowID(window.get()))
                minimized = false;
            break;
        }
    }
}

void App::Render() {}

bool App::ShouldRenderFrame() const noexcept
{
    return isRunning && window && !minimized &&
           !(SDL_GetWindowFlags(window.get()) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN));
}

void App::FinishFrame(Uint64 startNS, bool presented) const noexcept
{
    if (!isRunning || !sdlInitialized) return;
    const auto delay = Time::FrameDelayNS(SDL_GetTicksNS() - startNS, vsyncEnabled, presented);
    if (delay) SDL_DelayNS(delay);
}

void App::PostRender()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetIO().DisplaySize.y - 200));
    ImGui::SetNextWindowSize({ImGui::GetIO().DisplaySize.x, 200});
    ImGui::SetNextWindowBgAlpha(1.f);
    Terminal::Get().Render();

    ImGui::Render();
    // ImGui uses window coordinates, not the game's letterboxed logical space.
    Graphics().SetLogicalSize(0, 0);
    Graphics().SetScale(io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer.get());
    Graphics().Present();
}

void App::PreRender()
{
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    ImGui::BeginMainMenuBar();
    ImGui::MenuItem("Game");
    ImGui::MenuItem("Editors");
    if (ImGui::BeginMenu("Debug"))
    {
        ImGui::Checkbox("Toggle colliders", &toggleColliders);
        if (ImGui::Button("Terminal"))
            Terminal::Get().TurnOn();
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    // Rendering
    Graphics().SetLogicalSize(logicalWidth, logicalHeight);
    Graphics().SetScale(1.f, 1.f);
    Graphics().Clear({255, 255, 255, 255});
}

void App::StopAudioPlayback() noexcept
{
    if (audioOpened)
    {
        audio.StopAll();
    }
}

void App::Clean() noexcept
{
    isRunning = false;
    StopAudioPlayback();
    resources.reset();
    if (imguiContext)
    {
        auto* previousContext = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(imguiContext);
        if (imguiRendererInitialized)
        {
            ImGui_ImplSDLRenderer3_Shutdown();
            imguiRendererInitialized = false;
        }
        if (imguiPlatformInitialized)
        {
            ImGui_ImplSDL3_Shutdown();
            imguiPlatformInitialized = false;
        }
        ImGui::DestroyContext(imguiContext);
        if (previousContext != imguiContext)
            ImGui::SetCurrentContext(previousContext);
        imguiContext = nullptr;
        io = nullptr;
    }
    if (audioOpened)
    {
        audio.Close();
        audioOpened = false;
    }
    graphics.reset();
    renderer.reset();
    window.reset();
    if (sdlInitialized)
    {
        SDL_Quit();
        sdlInitialized = false;
    }
    toggleColliders = false;
    minimized = false;
    vsyncEnabled = false;
    inputAdapter.Reset();
    Time::ResetFrameClock();
}
