#pragma once
#include <SDL_mixer.h>
#include <string>

class Music
{
public:
    Music(const std::string& fileName);
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;
    void Play(unsigned int times = 1);
    ~Music();

private:
    // Exclusively owned; playback must stop before this wrapper is destroyed.
    Mix_Music* music = NULL;
};
