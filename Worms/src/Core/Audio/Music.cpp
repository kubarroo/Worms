#include "Music.h"
#include "ExceptionHandling/SDL_Exception.h"

Music::Music(const std::string& fileName)
{
    music = Mix_LoadMUS(fileName.c_str());
    if (!music)
    {
        const auto message = "Could not load music '" + fileName + "': " + Mix_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
}

void Music::Play(unsigned int times)
{
    Mix_PlayMusic(music, times);
}

Music::~Music()
{
    Mix_FreeMusic(music);
}
