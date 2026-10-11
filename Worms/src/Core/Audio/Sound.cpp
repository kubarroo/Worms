#include "Sound.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <memory>

Sound::Sound(MIX_Mixer* mixer, const std::string& fileName)
{
    std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)> source(
        SDL_IOFromFile(fileName.c_str(), "rb"), SDL_CloseIO);
    sound = source ? MIX_LoadAudio_IO(mixer, source.get(), true, false) : nullptr;
    if (!sound)
    {
        const auto message = "Could not load sound '" + fileName + "': " + SDL_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
}

Sound::~Sound()
{
    MIX_DestroyAudio(sound);
}
