#include "Sound.h"
#include "ExceptionHandling/SDL_Exception.h"
#include <memory>

Sound::Sound(const std::string& fileName)
{
    // Some mixer error paths do not close an owned input stream.
    std::unique_ptr<SDL_RWops, decltype(&SDL_RWclose)> source(
        SDL_RWFromFile(fileName.c_str(), "rb"), SDL_RWclose);
    sound = source ? Mix_LoadWAV_RW(source.get(), 0) : nullptr;
    if (!sound)
    {
        const auto message = "Could not load sound '" + fileName + "': " + Mix_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
}

Sound::~Sound()
{
    Mix_FreeChunk(sound);
}
