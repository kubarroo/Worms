#pragma once
#include <SDL_mixer.h>
#include <string>

class Sound
{
public:
    Sound(const std::string& fileName);
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    ~Sound();

private:
    friend class Audio;
    // Exclusively owned; playback must stop before this wrapper is destroyed.
    Mix_Chunk* sound = NULL;
};
