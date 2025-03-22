
#include <stdint.h>
#include "constants.h"
#include "frame.h"

enum
{
	OPCODE_DEVICE_STATE = 1,
	OPCODE_FRAME_BEGIN_DEPRECATED = 10,
    OPCODE_FRAME_END = 11,
    OPCODE_FRAME_ROW = 12,
	OPCODE_FRAME_BEGIN = 13,
	OPCODE_FULL_FRAME = 14,
	OPCODE_AUDIO_FRAME = 20,
    OPCODE_AUDIO_CHANGE = 21,
	OPCODE_AUDIO_OFFSET = 22,
	OPCODE_APPLICATION = 0x99,
};

const uint8_t APPLICATION_COMMAND_RESET = 0;
const uint8_t APPLICATION_COMMAND_1BIT_PALETTE = 1;
const uint8_t APPLICATION_COMMAND_4BIT_PALETTE = 2;

typedef struct
{
	uint8_t opcode;
	uint8_t unused;
	uint16_t payload_length;
} MessageHeader;

typedef struct
{
	uint8_t buttonMask;
	uint8_t stateBits;
	uint16_t unused;
	float crankAngle;
} MessageDeviceState;

typedef struct
{
	uint32_t timestamp_ms; // ms since beginning of the stream
} MessageFrameBegin;

typedef struct
{
	uint8_t rowNum;
	uint8_t data[ROW_SIZE_BYTES];
	uint8_t zero;
} MessageRowData;

typedef struct
{
	uint32_t timestamp_ms; // ms since beginning of the stream
	uint8_t rowmask[FRAME_HEIGHT/8];
	uint8_t data[]; // ROW_SIZE_BYTES * rowmask popcount
} MessageFrameData;

typedef struct
{
	RGB palette[2];
} Message1bitPalette;

typedef struct
{
	RGB palette[16];
} Message4bitPalette;

typedef struct
{
	uint8_t data[0];
} MessageAudioFrame;

typedef struct
{
	uint16_t flags;
} MessageAudioChange;

typedef struct
{
	uint32_t offset_samples;
} MessageAudioOffset;

#define STREAM_AUDIO_FLAG_ENABLED (1 << 0)
#define STREAM_AUDIO_FLAG_STEREO (1 << 1)

enum StreamAudioConfig 
{
	kAudioDisabled,
	kAudioStereo16,
	kAudioMono16,
};

enum StreamAudioMute
{
	kAudioMuteOn,
	kAudioMuteOff
};

bool stream_init();
void stream_begin();
void stream_poke();
void stream_addData(uint8_t* buf, unsigned int len);
bool stream_process();
void stream_reset();

void stream_sendButtonPress(int btn);
void stream_sendButtonRelease(int btn);
void stream_sendCrankChange(float angle_change);
