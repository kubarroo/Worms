#include "Core/Initialization/App.h"
#include "Core/Input.h"
#include "Core/ResourceManager.h"
#include "Core/Time.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <imgui_impl_sdlrenderer2.h>
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
        graphics = std::make_unique<Renderer2D>(renderer.get());
        graphics->SetLogicalSize(width, height);
        inputAdapter.Reset();
        resources = std::make_unique<ResourceManager>(renderer.get(), assetRoot);
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
    // Initialises the SDL video subsystem (as well as the events subsystem).
    try
    {
        SDL_CALL(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO));
    }
    catch (...)
    {
        SDL_Quit();
        throw;
    }
    sdlInitialized = true;

    /* Creates a SDL window */
    window.reset(SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  width, height, 0));
    SDL_CHECK(window.get());

    renderer.reset(SDL_CreateRenderer(window.get(), -1,
                                      SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC));
    SDL_CHECK(renderer.get());

    SDL_CALL(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096));
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
    imguiPlatformInitialized = ImGui_ImplSDL2_InitForSDLRenderer(window.get(), renderer.get());
    if (!imguiPlatformInitialized)
        throw std::runtime_error("Could not initialize ImGui SDL2 backend");
    imguiRendererInitialized = ImGui_ImplSDLRenderer2_Init(renderer.get());
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
        ImGui_ImplSDL2_ProcessEvent(&ev);
        inputAdapter.ProcessEvent(ev, io->WantCaptureKeyboard);
        switch (ev.type)
        {
        case SDL_QUIT:
            isRunning = false;
            break;
        }
    }
}

void App::Render() {}

void App::PostRender()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, ImGui::GetIO().DisplaySize.y - 200));
    ImGui::SetNextWindowSize({ImGui::GetIO().DisplaySize.x, 200});
    ImGui::SetNextWindowBgAlpha(1.f);
    Terminal::Get().Render();

    ImGui::Render();
    Graphics().SetScale(io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer.get());
    Graphics().Present();
}

void App::PreRender()
{
    // Start the Dear ImGui frame
    ImGui_ImplSDLRenderer2_NewFrame();
    ImGui_ImplSDL2_NewFrame();
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
            ImGui_ImplSDLRenderer2_Shutdown();
            imguiRendererInitialized = false;
        }
        if (imguiPlatformInitialized)
        {
            ImGui_ImplSDL2_Shutdown();
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
        Mix_CloseAudio();
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
    inputAdapter.Reset();
    Time::ResetFrameClock();
}
