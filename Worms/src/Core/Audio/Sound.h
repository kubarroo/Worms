#pragma once
#include <SDL3_mixer/SDL_mixer.h>
#include <string>

class Sound
{
public:
    Sound(MIX_Mixer* mixer, const std::string& fileName);
    Sound(const Sound&) = delete;
    Sound& operator=(const Sound&) = delete;
    ~Sound();

private:
    friend class Audio;
    // Exclusively owned; playback must stop before this wrapper is destroyed.
    MIX_Audio* sound = nullptr;
};
