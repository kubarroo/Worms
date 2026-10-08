#include "Core/Initialization/App.h"
#include "Core/Input.h"
#include "Core/Physics/ColliderFactory.h"
#include "Core/Physics/ContactManager.h"
#include "Core/Time.h"
#include "Terminal/Terminal.h"
#include "imgui_impl_sdl2.h"
#include <SDL2/SDL.h>
#include <SDL_mixer.h>
#include <imgui_impl_sdlrenderer2.h>


App::App() {}

void App::InitWindow(const std::string& title, const int width, const int height)
{
    InitSDL(title, width, height);
    InitImGui();

    isRunning = true;
}

void App::InitSDL(const std::string& title, const int width, const int height)
{
    // Initialises the SDL video subsystem (as well as the events subsystem).
    SDL_CALL(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0);

    /* Creates a SDL window */
    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width,
                              height, 0);
    SDL_CHECK(window);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_CHECK(renderer);

    SDL_CALL(Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 2, 4096));

    SDL_RenderSetLogicalSize(renderer, width, height);
}

void App::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    io = &ImGui::GetIO();
    (void)*io;
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
}

void App::Update()
{
    Terminal::Get().Update();
    ContactManager::Get().Update();
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
    if (toggleColliders)
        physicsWorld->DebugDraw();
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
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    SDL_DestroyWindow(window);
    SDL_DestroyRenderer(renderer);
    SDL_Quit();
}
