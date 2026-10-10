#include "Core/Initialization/App.h"
#include "Core/Input.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <imgui_impl_sdlrenderer2.h>
#include <stdexcept>


App::App() {}

void App::InitWindow(const std::string& title, const int width, const int height)
{
    if (sdlInitialized || imguiContext)
        throw std::logic_error("App is already initialized");
    InitSDL(title, width, height);
    InitImGui();

    isRunning = true;
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
    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                              height, 0);
    SDL_CHECK(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_CHECK(renderer);

    SDL_CALL(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096));
    audioOpened = true;

    SDL_CALL(SDL_RenderSetLogicalSize(renderer, width, height));
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
    imguiPlatformInitialized = ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    if (!imguiPlatformInitialized)
        throw std::runtime_error("Could not initialize ImGui SDL2 backend");
    imguiRendererInitialized = ImGui_ImplSDLRenderer2_Init(renderer);
    if (!imguiRendererInitialized)
        throw std::runtime_error("Could not initialize ImGui renderer backend");
}

void App::Update()
{
    Terminal::Get().Update();
}

void App::HandleEvents()
{
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        ImGui_ImplSDL2_ProcessEvent(&ev);
        switch (ev.type)
        {
        case SDL_QUIT:
            isRunning = false;
            break;
        case SDL_KEYUP:
            Input::Get().UpdateInputsUp(ev);
            break;
        case SDL_KEYDOWN:
            Input::Get().UpdateInputsDown(ev);
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
    SDL_RenderSetScale(renderer, io->DisplayFramebufferScale.x, io->DisplayFramebufferScale.y);
    ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
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
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
}

void App::Clean()
{
    isRunning = false;
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
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
        Mix_CloseAudio();
        audioOpened = false;
    }
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
    SDL_DestroyWindow(window);
    window = nullptr;
    if (sdlInitialized)
    {
        SDL_Quit();
        sdlInitialized = false;
    }
}
