#include "Sound.h"
#include "Terminal/Terminal.h"

Sound::Sound(const std::string& fileName)
{
    sound = Mix_LoadWAV(fileName.c_str());
    if (sound == NULL)
        Terminal::Get().Log("Coudn't load \"" + fileName + "\" as sound obj", LogLevel::ERROR);
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
