#pragma once
#include <SDL3_mixer/SDL_mixer.h>
#include <array>
class Sound;
class Music;
class Audio
{
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;
    void Open();
    void Close() noexcept;
    MIX_Mixer* Native() const noexcept { return mixer; }
    bool IsOpen() const noexcept { return mixer != nullptr; }
    int PlayingSounds() const noexcept;
    bool PlayingMusic() const noexcept;
    // Total plays: 1 means once; -1 means indefinitely. Zero is invalid.
    int Play(const Sound& sound, int plays = 1) const;
    void Play(const Music& music, int plays = 1) const;
    void StopSounds() const noexcept;
    void StopMusic() const noexcept;
    void StopAll() const noexcept;
    void SetSoundVolume(float volume) const;
    void SetMusicVolume(float volume) const;

private:
    MIX_Mixer* mixer = nullptr;
    std::array<MIX_Track*, 8> sounds{};
    MIX_Track* music = nullptr;
    bool initialized = false;
    mutable float soundVolume = 1;
    mutable float musicVolume = 1;
    void Start(MIX_Track* track, MIX_Audio* asset, int plays) const;
    static void ValidateVolume(float volume);
};
