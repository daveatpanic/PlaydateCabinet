//
//  ringbuffer.h
//  Playdate Simulator
//
//  Created by Dave Hayden on 5/21/18.
//  Copyright © 2018 Panic, Inc. All rights reserved.
//

#ifndef ringbuffer_h
#define ringbuffer_h

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	void* buffer;
	unsigned int bufferlen;
	unsigned int datasize; // number of bytes in a frame
	unsigned int inpos; // offset of the input/read pointer (this moves when we read data from disk)
	unsigned int outpos; // offset of the output pointer (this moves when we push audio to output)
} RingBuffer;

void RingBuffer_init(RingBuffer* r);
void RingBuffer_deinit(RingBuffer* r);

bool RingBuffer_setSize(RingBuffer* r, unsigned int bytes, unsigned int datasize);
unsigned int RingBuffer_getSize(RingBuffer* r);

// clear inpos, outpos, and fileOffset
void RingBuffer_reset(RingBuffer* r);

unsigned int RingBuffer_getFreeSpace(RingBuffer* r);
unsigned int RingBuffer_getBytesAvailable(RingBuffer* r);

// returns number of bytes actually copied, if length is > space available
unsigned int RingBuffer_addData(RingBuffer* r, const void* data, unsigned int length);

// returns number of bytes actually copied, if bytes > available data
unsigned int RingBuffer_readData(RingBuffer* r, void* data, unsigned int bytes);

// direct access to buffer data:
unsigned int RingBuffer_getInputAvailableSize(RingBuffer* r); // contiguous space
void* RingBuffer_getInputPointer(RingBuffer* r);
void RingBuffer_moveInputPointer(RingBuffer* r, unsigned int bytes);

unsigned int RingBuffer_getOutputAvailableSize(RingBuffer* r); // contiguous space
void* RingBuffer_getOutputPointer(RingBuffer* r);
void RingBuffer_moveOutputPointer(RingBuffer* r, unsigned int bytes);

#endif /* ringbuffer_h */
