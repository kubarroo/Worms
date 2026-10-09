#pragma once
#include <SDL_mixer.h>
#include <string>

class Sound
{
public:
    Sound(const std::string& fileName);
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    void Play(unsigned int times = 1);
    ~Sound();

private:
    Mix_Chunk* sound = NULL;
};
