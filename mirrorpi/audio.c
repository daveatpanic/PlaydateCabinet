//
//  audio.c
//  MirrorJr
//
//  Created by Dave Hayden on 9/25/24.
//

#include "SDL.h"
#include "assert.h"

#include "audio.h"
#include "ringbuffer.h"

#define LOG printf
//#define LOG(s)

#define AUDIO_SAMPLE_RATE 44100
#define BUFFER_SIZE 32768
unsigned int silentcount = BUFFER_SIZE;

static RingBuffer buffer;

SDL_AudioDeviceID soundDevice;
const int SDL_FRAME_SIZE = sizeof(int16_t) * 2;
unsigned int num_channels = 2;

void SDLAudioCallback(void* userdata, Uint8* stream, int len)
{
	if ( silentcount >= BUFFER_SIZE / (sizeof(int16_t) * num_channels) )
	{
		memset(stream, 0, (unsigned)len);
		return;
	}
	
	if ( num_channels == 1 )
	{
		int16_t* stream16 = (int16_t*)stream;
		int16_t s;

		for ( int i = 0; i < len / SDL_FRAME_SIZE; ++i )
		{
			if ( RingBuffer_readData(&buffer, &s, 2) != 2 )
				break;
			
			stream16[2*i] = stream16[2*i+1] = s;
		}
	}
	else
		RingBuffer_readData(&buffer, stream, (unsigned)len);
}

bool audio_init()
{
	RingBuffer_init(&buffer);
	RingBuffer_setSize(&buffer, BUFFER_SIZE, SDL_FRAME_SIZE);

	if ( SDL_InitSubSystem(SDL_INIT_AUDIO) != 0 )
	{
		printf("audio init failed: %s", SDL_GetError());
		return false;
	}
	
	int numdevs = SDL_GetNumAudioDevices(0);
	
	for ( int i = 0; i < numdevs; ++i )
	{
		SDL_AudioSpec spec;
		SDL_GetAudioDeviceSpec(i, 0, &spec);
		printf("audio dev %i: %s\n", i, SDL_GetAudioDeviceName(i, 0));
	}
	
	SDL_AudioSpec want, have;
	SDL_memset(&want, 0, sizeof(want));

	want.freq = AUDIO_SAMPLE_RATE;
	want.format = AUDIO_S16LSB;
	want.channels = 2;
	want.samples = 256 * SDL_FRAME_SIZE;
	want.callback = SDLAudioCallback;

	soundDevice = SDL_OpenAudioDevice
	(
		NULL,
		0, // playback
		&want,
		&have,
		0); // do not SDL_AUDIO_ALLOW_FORMAT_CHANGE

	LOG("soundDevice: %d\n", soundDevice);
	
	return true;
}

void audio_setFormat(unsigned int channels)
{
	num_channels = channels;
}

static bool running = false;

void audio_addData(uint8_t* data, unsigned int len)
{
	unsigned int avail = RingBuffer_getFreeSpace(&buffer);
	
	if ( avail < len )
	{
		printf("audio buffer overflowed\n");
		len = avail;
	}
	
	RingBuffer_addData(&buffer, data, len);
	silentcount = 0;
	
	if ( !running )
	{
		SDL_PauseAudioDevice(soundDevice, 0);
		running = true;
	}
}

void audio_stop()
{
	SDL_PauseAudioDevice(soundDevice, 1);
	RingBuffer_reset(&buffer);
	running = false;
}

#define MIN(a,b) (((a)<(b))?(a):(b))

void audio_addSilence(unsigned int len)
{
	if ( silentcount < BUFFER_SIZE / num_channels )
	{
		silentcount += len;
		
		while ( len > 0 )
		{
			static const int16_t zeros[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
			unsigned int l = MIN(len, sizeof(zeros));
			RingBuffer_addData(&buffer, zeros, l);
			len -= l;
		}
	}
	else // buffer is all zero, can just push ringbuffer pointer forward
		RingBuffer_moveInputPointer(&buffer, len);
}
