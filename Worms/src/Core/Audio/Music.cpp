#include "Music.h"
#include "ExceptionHandling/SDL_Exception.h"

Music::Music(MIX_Mixer* mixer, const std::string& fileName)
{
    music = MIX_LoadAudio(mixer, fileName.c_str(), false);
    if (!music)
    {
        const auto message = "Could not load music '" + fileName + "': " + SDL_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
}

Music::~Music()
{
    MIX_DestroyAudio(music);
}
