#include <fstream>

#include <stdio.h> /* printf and fprintf */
#include "Core/Initialization/App.h"
#include "Core/Time.h"
#include "Game/Game.h"
#include "Terminal/Terminal.h"

#include <filesystem>
#include <memory>
#include <system_error>

/* Sets constants */
#define WIDTH 800
#define HEIGHT 600

int main( int argc, char** argv )
{
	std::unique_ptr<char, decltype(&SDL_free)> basePath(
		SDL_GetBasePath(), &SDL_free
	);

	if (!basePath) {
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR, "Startup error", SDL_GetError(), nullptr
		);
		return 1;
	}

	std::error_code error;
	std::filesystem::current_path(
		std::filesystem::u8path(basePath.get()), error
	);

	if (error) {
		SDL_ShowSimpleMessageBox(
			SDL_MESSAGEBOX_ERROR, "Startup error",
			error.message().c_str(), nullptr
		);
		return 1;
	}

	std::unique_ptr<App> game = std::make_unique<Game>();
	Time::Timer timer{};

	try
	{
		game->InitWindow( "Worms", 800, 600 );

		while ( game->IsRunning() )
		{
			try
			{
				Time::deltaTime = timer.Reset();
				game->HandleEvents();
				game->Update();
				game->PreRender();
				game->Render();
				game->PostRender();
			}
			catch ( std::exception& e )
			{
				Terminal::Get().Log( e.what(), LogLevel::ERROR );
			}

		}

		game->Clean();
	}
	catch ( std::exception& e )
	{
		Terminal::Get().Log( e.what(), LogLevel::ERROR );
	}

	return 0;
}