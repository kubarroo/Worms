#pragma once
#include <SDL3_mixer/SDL_mixer.h>
#include <string>

class Music
{
public:
    Music(MIX_Mixer* mixer, const std::string& fileName);
    Music(const Music&) = delete;
    Music& operator=(const Music&) = delete;
    ~Music();

private:
    friend class Audio;
    // Exclusively owned; playback must stop before this wrapper is destroyed.
    MIX_Audio* music = nullptr;
};
