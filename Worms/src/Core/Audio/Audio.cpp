#include "Audio.h"
#include "ExceptionHandling/SDL_Exception.h"
#include "Music.h"
#include "Sound.h"
#include <cmath>
#include <stdexcept>

Audio::~Audio() { Close(); }

void Audio::Open()
{
    if (initialized) throw std::logic_error("Audio is already initialized");
    SDL_CALL(MIX_Init());
    initialized = true;
    try
    {
        mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        SDL_CHECK(mixer);
        for (auto& track : sounds)
        {
            track = MIX_CreateTrack(mixer);
            SDL_CHECK(track);
            SDL_CALL(MIX_SetTrackGain(track, soundVolume));
        }
        music = MIX_CreateTrack(mixer);
        SDL_CHECK(music);
        SDL_CALL(MIX_SetTrackGain(music, musicVolume));
    }
    catch (...)
    {
        Close();
        throw;
    }
}

void Audio::Close() noexcept
{
    StopAll();
    if (mixer) MIX_DestroyMixer(mixer); // The mixer owns its tracks.
    mixer = nullptr;
    sounds.fill(nullptr);
    music = nullptr;
    if (initialized) MIX_Quit();
    initialized = false;
}

void Audio::Start(MIX_Track* track, MIX_Audio* asset, int plays) const
{
    if (plays == 0 || plays < -1) throw std::invalid_argument("Invalid playback count");
    if (!mixer) throw std::logic_error("Playback requires an open audio device");
    SDL_CALL(MIX_SetTrackAudio(track, asset));
    if (plays == 1)
    {
        SDL_CALL(MIX_PlayTrack(track, 0));
        return;
    }
    const auto options = SDL_CreateProperties();
    SDL_CHECK(options);
    try
    {
        SDL_CALL(SDL_SetNumberProperty(options, MIX_PROP_PLAY_LOOPS_NUMBER,
                                       plays == -1 ? -1 : plays - 1));
        SDL_CALL(MIX_PlayTrack(track, options));
    }
    catch (...)
    {
        SDL_DestroyProperties(options);
        throw;
    }
    SDL_DestroyProperties(options);
}
int Audio::Play(const Sound& sound, int plays) const
{
    if (plays == 0 || plays < -1) throw std::invalid_argument("Invalid playback count");
    if (!mixer) throw std::logic_error("Playback requires an open audio device");
    for (std::size_t i = 0; i < sounds.size(); ++i)
        if (!MIX_TrackPlaying(sounds[i]))
        {
            Start(sounds[i], sound.sound, plays);
            return static_cast<int>(i);
        }
    return -1;
}
void Audio::Play(const Music& music, int plays) const
{
    Start(this->music, music.music, plays);
}
void Audio::StopSounds() const noexcept
{
    for (auto* track : sounds)
        if (track)
        {
            MIX_StopTrack(track, 0);
            MIX_SetTrackAudio(track, nullptr);
        }
}
void Audio::StopMusic() const noexcept
{
    if (music)
    {
        MIX_StopTrack(music, 0);
        MIX_SetTrackAudio(music, nullptr);
    }
}
void Audio::StopAll() const noexcept
{
    StopMusic();
    StopSounds();
}
int Audio::PlayingSounds() const noexcept
{
    int playing = 0;
    for (auto* track : sounds) if (track && MIX_TrackPlaying(track)) ++playing;
    return playing;
}
bool Audio::PlayingMusic() const noexcept { return music && MIX_TrackPlaying(music); }

void Audio::ValidateVolume(float volume)
{
    if (!std::isfinite(volume) || volume < 0 || volume > 1)
        throw std::invalid_argument("Volume must be between zero and one");
}
void Audio::SetSoundVolume(float volume) const
{
    ValidateVolume(volume);
    for (auto* track : sounds) if (track) SDL_CALL(MIX_SetTrackGain(track, volume));
    soundVolume = volume;
}
void Audio::SetMusicVolume(float volume) const
{
    ValidateVolume(volume);
    if (music) SDL_CALL(MIX_SetTrackGain(music, volume));
    musicVolume = volume;
}
