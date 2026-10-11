#include <fstream>

#include "Core/Initialization/App.h"
#include "Core/Time.h"
#include "Game/Game.h"
#include "Terminal/Terminal.h"
#include <stdio.h> /* printf and fprintf */


#include <filesystem>
#include <memory>
#include <system_error>
#include <SDL3/SDL_main.h>

/* Sets constants */
#define WIDTH 800
#define HEIGHT 600

int main(int argc, char** argv)
{
    const char* basePath = SDL_GetBasePath();

    if (!basePath)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Startup error", SDL_GetError(), nullptr);
        return 1;
    }

    std::error_code error;
    std::filesystem::current_path(std::filesystem::u8path(basePath), error);

    if (error)
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Startup error", error.message().c_str(),
                                 nullptr);
        return 1;
    }

    std::unique_ptr<App> game = std::make_unique<Game>();
    int exitCode = 0;

    try
    {
        game->InitWindow("Worms", 800, 600);

        while (game->IsRunning())
        {
            const auto frameStart = SDL_GetTicksNS();
            bool presented = false;
            try
            {
                Time::UpdateFrameTime();
                game->HandleEvents();
                if (!game->IsRunning()) break;
                game->Update();
                if (game->ShouldRenderFrame())
                {
                    game->PreRender();
                    game->Render();
                    game->PostRender();
                    presented = true;
                }
            }
            catch (std::exception& e)
            {
                Terminal::Get().Log(e.what(), LogLevel::ERROR);
            }
            catch (...)
            {
                Terminal::Get().Log("Unknown exception during game update", LogLevel::ERROR);
            }
            game->FinishFrame(frameStart, presented);
        }
    }
    catch (std::exception& e)
    {
        exitCode = 1;
        Terminal::Get().Log(e.what(), LogLevel::ERROR);
    }
    catch (...)
    {
        exitCode = 1;
        Terminal::Get().Log("Unknown exception during game initialization", LogLevel::ERROR);
    }

    game->Clean();
    return exitCode;
}
