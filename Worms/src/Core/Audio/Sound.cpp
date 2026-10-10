#include "Sound.h"
#include "ExceptionHandling/SDL_Exception.h"

Sound::Sound(const std::string& fileName)
{
    sound = Mix_LoadWAV(fileName.c_str());
    if (!sound)
    {
        const auto message = "Could not load sound '" + fileName + "': " + Mix_GetError();
        throw SDL_Exception(__LINE__, __FILE__, message.c_str());
    }
}

void Sound::Play(unsigned int times)
{
    if (sound != NULL)
        Mix_PlayChannel(-1, sound, times - 1);
}

Sound::~Sound()
{
    Mix_FreeChunk(sound);
}
