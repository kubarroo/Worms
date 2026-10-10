#include "Music.h"
#include "ExceptionHandling/SDL_Exception.h"

Music::Music(const std::string& fileName)
{
    music = Mix_LoadMUS(fileName.c_str());
    SDL_CHECK(music);
}

void Music::Play(unsigned int times)
{
    Mix_PlayMusic(music, times);
}

Music::~Music()
{
    Mix_FreeMusic(music);
}
