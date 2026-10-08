#include "Music.h"
#include "Terminal/Terminal.h"

Music::Music(const std::string& fileName)
{
    music = Mix_LoadMUS(fileName.c_str());
    if (music == NULL)
        Terminal::Get().Log("Coudn't load \"" + fileName + "\" as music obj \n" + SDL_GetError(),
                            LogLevel::ERROR);
}

void Music::Play(unsigned int times)
{
    Mix_PlayMusic(music, times);
}

Music::~Music()
{
    Mix_FreeMusic(music);
}
