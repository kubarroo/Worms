#include "Audio.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Music.h"
#include "Sound.h"
#include <cmath>
#include <stdexcept>

int Audio::Loops(int plays)
{
    if (plays == 0 || plays < -1)
        throw std::invalid_argument("Invalid playback count");
    if (!Mix_QuerySpec(nullptr, nullptr, nullptr))
        throw std::logic_error("Playback requires an open audio device");
    return plays == -1 ? -1 : plays - 1;
}
int Audio::Play(const Sound& sound, int plays) const
{
    // No free channel is a normal condition; callers may inspect the returned -1.
    return Mix_PlayChannel(-1, sound.sound, Loops(plays));
}
void Audio::Play(const Music& music, int plays) const
{
    SDL_CALL(Mix_PlayMusic(music.music, Loops(plays)));
}
void Audio::StopSounds() const noexcept
{
    if (Mix_QuerySpec(nullptr, nullptr, nullptr))
        Mix_HaltChannel(-1);
}
void Audio::StopMusic() const noexcept
{
    if (Mix_QuerySpec(nullptr, nullptr, nullptr))
        Mix_HaltMusic();
}
void Audio::StopAll() const noexcept
{
    StopMusic();
    StopSounds();
}
int Audio::Volume(float volume)
{
    if (!std::isfinite(volume) || volume < 0 || volume > 1)
        throw std::invalid_argument("Volume must be between zero and one");
    return static_cast<int>(volume * MIX_MAX_VOLUME);
}
void Audio::SetSoundVolume(float volume) const
{
    Mix_Volume(-1, Volume(volume));
}
void Audio::SetMusicVolume(float volume) const
{
    Mix_VolumeMusic(Volume(volume));
}
