
#include <stdbool.h>
#include <unistd.h>
#include "stream.h"
#include "serial.h"
#include "frame.h"
#include "audio.h"
#include "ringbuffer.h"

#define LOG printf
//#define LOG(...)

//static const char ECHO_ON[] = "echo on\r\n";
//static const char STREAM[] = "stream\r\n";
static const char STREAM_ENABLE[] = "stream enable\r\n";
static const char STREAM_FULLFRAME[] = "stream fullframe\r\n";
static const char STREAM_DISABLE[] = "stream disable\r\n";
static const char STREAM_POKE[] = "stream poke\r\n";

enum StreamProtocolState {
	kStreamDisabled,
	kStreamEnabling,
	kStreamStreamStarting,
	kStreamParsingFirstHeader,
	kStreamParsingHeader,
	kStreamParsingPayload,
	kStreamParsingPoke
} state;

#define MAX_PAYLOAD_SIZE (sizeof(MessageFrameData) + FRAME_SIZE_BYTES)
uint8_t payload[MAX_PAYLOAD_SIZE];
int payloadBytesRead = 0;

enum StreamAudioConfig audio_config = kAudioStereo16;
enum StreamAudioMute audio_mute = kAudioMuteOff;

//! String to expect back on enable
int expectedBytesRead = 0;

//	wxTimer enable_timer_{this, TIMER_StreamProtocolEnable};
//	wxTimer start_timer_{this, TIMER_StreamProtocolStart};
//	wxTimer poke_timer_{this, TIMER_StreamProtocolPoke};

//void streamEnabled();

int64_t stream_start_ms = 0;
RingBuffer serialbuf;
#define BUFFER_SIZE 65536
unsigned int buffer_highwater = 0;

void handleStreamMessage(MessageHeader *hdr);

bool stream_init()
{
	RingBuffer_init(&serialbuf);
	RingBuffer_setSize(&serialbuf, BUFFER_SIZE, 1);
	
	//usleep(10000);
	
	// try to flush input
	/*
	int n;
	char buf[256];
	
	while ( (n = ser_readNonblocking(buf, sizeof(buf))) > 0 )
		;

	return n == 0;
	*/
	return true;
}

void sendStreamOption(const char* opt)
{
	char buf[32];
	snprintf(buf, 32, "stream %s\r\n", opt);
	ser_writeNonblocking(buf, strlen(buf));
}

void disableStream()
{
	if ( state != kStreamDisabled )
	{
		//poke_timer.Stop();
		ser_writestr(STREAM_DISABLE);
		state = kStreamDisabled;
	}
}

void disconnect()
{
	disableStream();
	LOG("Closing serial port\n");
	ser_close();
}

void sendStreamEnable()
{
	expectedBytesRead = 0;
	printf("resetting stream\n");
	state = kStreamEnabling;
	//ser_writestr(ECHO_ON);
	ser_writestr(STREAM_ENABLE);
	
	// enable full frame updates and application commands
	ser_writestr(STREAM_FULLFRAME);
}

void reconnect()
{
	disableStream();
	LOG("Flushing connection\n");
	usleep(10000);
	ser_flush();
	LOG("Re-enabling stream\n");
	sendStreamEnable();
}

void sendButtonPress(int button);
void sendButtonRelease(int button);
void sendCrankEnable(bool enable);
void sendCrankChange(double angle_change);
//void sendAccelChange(double x, double y, double z);

void stream_poke()
{
	// let device know we're still here
	ser_writeNonblocking(STREAM_POKE, strlen(STREAM_POKE));
}

const char* getOptionString(enum StreamAudioConfig cfg)
{
	switch ( cfg )
	{
		case kAudioDisabled: return "a-";
		case kAudioStereo16: return "a+";
		case kAudioMono16: return "am";
		default: return NULL;
	}
}

void setAudioConfiguration(enum StreamAudioConfig cfg)
{
	if ( cfg == audio_config )
		return;

	audio_config = cfg;

	if ( state != kStreamDisabled )
	{
		const char* s = getOptionString(cfg);

		if ( s != NULL )
			sendStreamOption(s);
	}
}

void setMuteValue(enum StreamAudioMute value)
{
	audio_mute = value;
	ser_writestr(value == kAudioMuteOn ? "mute on\r\n" : "mute off\r\n");
}

uint32_t swap_bits(uint8_t n)
{
	n = ((n & 0x55) << 1) | ((n & 0xaa) >> 1);
	n = ((n & 0x33) << 2) | ((n & 0xcc) >> 2);
	n = ((n & 0x0f) << 4) | ((n & 0xf0) >> 4);
	return n;
}

bool prv_is_valid_header(MessageHeader hdr)
{
	const uint16_t sz = hdr.payload_length;
	
	switch ( hdr.opcode )
	{
		case OPCODE_DEVICE_STATE:
			return (sz == sizeof(MessageDeviceState));
		case OPCODE_FRAME_BEGIN_DEPRECATED:
			return (sz == 0);
		case OPCODE_FRAME_BEGIN:
			return (sz == sizeof(MessageFrameBegin));
		case OPCODE_FRAME_END:
			return (sz == 0);
		case OPCODE_FRAME_ROW:
			return (sz == sizeof(MessageRowData));
		case OPCODE_FULL_FRAME:
		{
			unsigned int rows = hdr.unused;
			return (sz == sizeof(MessageFrameData) + rows * 50);
		}
		case OPCODE_AUDIO_FRAME:
			return (sz < 2048);
		case OPCODE_AUDIO_CHANGE:
			return (sz == sizeof(MessageAudioChange));
		case OPCODE_AUDIO_OFFSET:
			return (sz == sizeof(MessageAudioOffset));
		case OPCODE_APPLICATION:
		{
			if ( hdr.unused == APPLICATION_COMMAND_RESET )
				return (sz == 0);
			else if ( hdr.unused == APPLICATION_COMMAND_1BIT_PALETTE )
				return (sz == sizeof(Message1bitPalette));
			else if ( hdr.unused == APPLICATION_COMMAND_4BIT_PALETTE )
				return (sz == sizeof(Message4bitPalette));
			else
				return true;
		}
		case 's':
			return (sz == 25970); // "stream poke"
	}
	return false;
}

const char* to_string(enum StreamAudioConfig cfg)
{
	switch (cfg)
	{
		case kAudioDisabled:	return "disabled";
		case kAudioStereo16:	return "stereo16";
		case kAudioMono16:		return "mono16";
		default:				return "???";
	}
}

enum StreamAudioConfig toStreamAudioConfig(const char* s)
{
	if ( strcmp(s, "stereo16") == 0 )
		return kAudioStereo16;
	else if ( strcmp(s, "mono16") == 0 )
		return kAudioMono16;
	else
		return kAudioDisabled;
}

enum StreamAudioConfig flagsToStreamAudioConfig(uint8_t flags)
{
	if ( (flags & STREAM_AUDIO_FLAG_ENABLED) == 0 )
		return kAudioDisabled;
	else if ( flags & STREAM_AUDIO_FLAG_STEREO )
		return kAudioStereo16;
	else
		return kAudioMono16;
}

void streamStarted()
{
	//stream_start_ms = wxGetUTCTimeMillis().GetValue();

	const char* opt = getOptionString(audio_config);
	
	// Try to enable audio if needed & supported by device
	if ( opt != NULL )
		sendStreamOption(opt);
	
	setMuteValue(audio_mute);
}

static void handleStreamPayload();

MessageHeader header;
int headerBytesRead = 0;
int pokeBytesRead = 0;

#define MAX(a,b) (((a)>(b))?(a):(b))

void stream_addData(uint8_t* buf, unsigned int len)
{
	for ( ;; )
	{
		unsigned int n = RingBuffer_addData(&serialbuf, buf, len);
		unsigned int avail = RingBuffer_getBytesAvailable(&serialbuf);
		
		if ( avail > buffer_highwater )
		{
			buffer_highwater = avail;
			printf("serial buf max: %i bytes\n", avail);
		}

		if ( (len -= n) == 0 )
			break;
		
		buf += n;
		printf("serial ringbuffer full\n");
		usleep(1000);
	}
}

bool stream_processbuf(uint8_t* buf, unsigned int len)
{
	uint8_t* end = buf + len;
	
	while ( buf < end )
	{
		switch ( state ) 
		{
			case kStreamDisabled:
				return true;
	
			case kStreamEnabling:
			{
				while ( buf < end && expectedBytesRead < strlen(STREAM_ENABLE) )
				{
					if ( *buf++ == STREAM_ENABLE[expectedBytesRead] )
						++expectedBytesRead;
					else
						expectedBytesRead = 0;
				}
				
				if ( expectedBytesRead == strlen(STREAM_ENABLE) )
				{
					printf("stream enable received\n");
					//streamEnabled();
					state = kStreamStreamStarting;
				}

				break;
			}
			case kStreamStreamStarting:
			{
				// XXX - first opcode received is 0x0d = \n :(
				
				// skip errant \r and \n
				//if ( *buf != '\r' && *buf != '\n' )
				//{
					headerBytesRead = 0;
					state = kStreamParsingFirstHeader;
				//}
				//else
				//	++buf;
					
				break;
			}
			case kStreamParsingFirstHeader:
			case kStreamParsingHeader:
			{
				while ( buf < end && headerBytesRead < sizeof(MessageHeader) )
					((uint8_t*)&header)[headerBytesRead++] = *buf++;
				
				if ( headerBytesRead < sizeof(MessageHeader) )
					return true;
				
				if ( strncmp((char*)&header, "stre", 4) == 0 )
				{
					printf("read \"stre\", scanning for \"stream poke\"\n");
					pokeBytesRead = 4;
					state = kStreamParsingPoke;
					continue;
				}

				if ( !prv_is_valid_header(header) )
				{
					LOG("Invalid header opcode %d length %d, reconnecting\n", header.opcode, header.payload_length);
//					exit(0);
					reconnect();
					return false;
				}
			
				//LOG("Received opcode %u sz %u\n", header.opcode, header.payload_length);

				if ( state == kStreamParsingFirstHeader )
					streamStarted();

				payloadBytesRead = 0;
				state = kStreamParsingPayload;
				
				break;
			}
			case kStreamParsingPayload:
			{
				const size_t payload_size = header.payload_length;
				
				while ( buf < end && payloadBytesRead < payload_size )
					payload[payloadBytesRead++] = *buf++;

				if ( payload_size > 0 && payloadBytesRead < payload_size )
					return true;
				
				//LOG("Finished parsing message\n");
				handleStreamPayload();
				
				headerBytesRead = 0;
				state = kStreamParsingHeader;
				break;
			}
			case kStreamParsingPoke:
			{
				int len = strlen(STREAM_POKE);
				
//				while ( buf < end && pokeBytesRead < len && *buf == STREAM_POKE[pokeBytesRead] )
//					++pokeBytesRead;

				while ( buf < end )
				{
					if ( pokeBytesRead < len )
					{
						printf("looking for char %i %c (%02x), got %c (%02x)\n", pokeBytesRead, STREAM_POKE[pokeBytesRead], STREAM_POKE[pokeBytesRead], *buf, *buf);
						
						if ( *buf == STREAM_POKE[pokeBytesRead] || *buf == STREAM_ENABLE[pokeBytesRead] )
						{
							++pokeBytesRead;
							++buf;
						}
						else
							break;
					}
					else
						break;
				}
				
				if ( pokeBytesRead == len )
				{
					printf("found \"stream poke\", back to payloads\n");
					headerBytesRead = 0;
					state = kStreamParsingHeader;
				}
				else if ( buf < end )
				{
					printf("failed reading stream poke\n");
					exit(0);
				}
				
				break;
			}
		}
	}
	
	return true;
}

bool stream_process()
{
	unsigned int n;
	
	while ( (n = RingBuffer_getOutputAvailableSize(&serialbuf)) > 0 )
	{
		if ( !stream_processbuf(RingBuffer_getOutputPointer(&serialbuf), n) )
			return false;
		
		RingBuffer_moveOutputPointer(&serialbuf, n);
	}
	
	return true;
}

static void handleStreamPayload()
{
	if ( header.opcode == OPCODE_FRAME_BEGIN_DEPRECATED )
	{
		int64_t ts_ms = 0; //wxGetUTCTimeMillis().GetValue() - stream_start_ms;
		frame_begin((uint32_t)ts_ms);
	}
	else if ( header.opcode == OPCODE_FRAME_BEGIN )
	{
		MessageFrameBegin* rb = (MessageFrameBegin*)&payload[0];
		frame_begin(rb->timestamp_ms);
	}
	else if ( header.opcode == OPCODE_FRAME_ROW )
	{
		MessageRowData* rd = (MessageRowData*)&payload[0];
		frame_setRow(swap_bits(rd->rowNum), rd->data);
	}
	else if ( header.opcode == OPCODE_FRAME_END )
	{
		frame_end();
	}
	else if ( header.opcode == OPCODE_FULL_FRAME )
	{
		MessageFrameData* fd = (MessageFrameData*)&payload[0];
		
		frame_begin(fd->timestamp_ms);
		
		for ( unsigned int i = 0, row = 0; i < FRAME_HEIGHT; ++i )
		{
			if ( fd->rowmask[i/8] & (1<<(i%8)) )
				frame_setRow(i+1, fd->data + 2 + ROW_SIZE_BYTES * row++);
		}
		
		frame_end();
	}
	else if ( header.opcode == OPCODE_AUDIO_CHANGE )
	{
		MessageAudioChange* ac = (MessageAudioChange*)&payload[0];
		unsigned int num_channels = (ac->flags & STREAM_AUDIO_FLAG_STEREO) ? 2 : 1;
		audio_setFormat(num_channels);
	}
	else if ( header.opcode == OPCODE_AUDIO_FRAME )
	{
		if ( audio_config != kAudioDisabled )
		{
			MessageAudioFrame* af = (MessageAudioFrame*)&payload[0];
			audio_addData(af->data, header.payload_length);
		}
	}
	else if ( header.opcode == OPCODE_AUDIO_OFFSET )
	{
		if ( audio_config != kAudioDisabled )
		{
			MessageAudioOffset* ao = (MessageAudioOffset*)&payload[0];
			audio_addSilence(ao->offset_samples);
		}
	}
	else if ( header.opcode == OPCODE_DEVICE_STATE )
	{
		MessageDeviceState* state = (MessageDeviceState*)&payload[0];
		static int lastdropped = -1;
		int dropped = state->unused;
		if ( dropped != lastdropped && lastdropped != -1 )
			printf("%i messages dropped\n", dropped>lastdropped ? dropped-lastdropped : dropped+65536-lastdropped);
		lastdropped = dropped;
	}
	else if ( header.opcode == OPCODE_APPLICATION )
	{
		// mode/palette change message
		if ( header.unused == APPLICATION_COMMAND_RESET )
			frame_reset();
		else if ( header.unused == APPLICATION_COMMAND_1BIT_PALETTE )
			frame_set1BitPalette((RGB*)payload);
		else if ( header.unused == APPLICATION_COMMAND_4BIT_PALETTE )
			frame_set4BitPalette((RGB*)payload);
	}
}

void stream_begin()
{
	ser_flush();
	sendStreamEnable();
	frame_reset();
	//stream_start_ms = wxGetUTCTimeMillis().GetValue();
}

void stream_reset()
{
	ser_flush();
	expectedBytesRead = 0;
	state = kStreamDisabled;
	RingBuffer_reset(&serialbuf);
}

static const char keynames[] = "lrudbam";

void stream_sendButtonPress(int btn)
{
	char buf[] = "btn +x\r\n";
	buf[5] = keynames[btn];
	printf("sending +%c\n", buf[5]);
	ser_writeNonblocking(buf, strlen(buf));
}

void stream_sendButtonRelease(int btn)
{
	char buf[] = "btn -x\r\n";
	buf[5] = keynames[btn];
	printf("sending -%c\n", buf[5]);
	ser_writeNonblocking(buf, strlen(buf));
}

void stream_sendCrankChange(float angle_change)
{
	char buf[32];
	snprintf(buf, 32, "changecrank %.1f\r\n", angle_change);
	ser_writeNonblocking(buf, strlen(buf));
}

void stream_sendAccelChange(double x, double y, double z)
{
	char buf[32];
	snprintf(buf, 32, "accel %i %i %i\r\n", (int)(x*1000), (int)(y*1000), (int)(z*1000));
	ser_writeNonblocking(buf, strlen(buf));
}

void stream_sendCrankEnable(bool enable)
{
	const char* msg = enable ? "enablecrank\r\n" : "disablecrank\r\n";
	ser_writeNonblocking(msg, strlen(msg));
}
