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
    Mix_Music* music = NULL;
};
