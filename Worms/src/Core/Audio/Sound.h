#pragma once
#include <SDL_mixer.h>
#include <string>

class Sound
{
public:
	Sound( const std::string& fileName );
	void Play( unsigned int times = 1 );
	~Sound();

private:
	Mix_Chunk* sound = NULL;
};

