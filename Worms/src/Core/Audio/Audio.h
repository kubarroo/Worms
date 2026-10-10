#pragma once
class Sound;
class Music;
class Audio
{
public:
    // Total plays: 1 means once; -1 means indefinitely. Zero is invalid.
    int Play(const Sound& sound, int plays = 1) const;
    void Play(const Music& music, int plays = 1) const;
    void StopSounds() const noexcept;
    void StopMusic() const noexcept;
    void StopAll() const noexcept;
    void SetSoundVolume(float volume) const;
    void SetMusicVolume(float volume) const;

private:
    static int Loops(int plays);
    static int Volume(float volume);
};
