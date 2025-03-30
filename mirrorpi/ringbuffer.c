//
//  ringbuffer.c
//  Playdate Simulator
//
//  Created by Dave Hayden on 5/21/18.
//  Copyright © 2018 Panic, Inc. All rights reserved.
//

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "ringbuffer.h"

void RingBuffer_init(RingBuffer* r)
{
	r->buffer = NULL;
	r->bufferlen = 0;
//	r->fileOffset = 0;
	r->inpos = 0;
	r->outpos = 0;
}

void RingBuffer_deinit(RingBuffer* r)
{
	if ( r->buffer != NULL )
		free(r->buffer);
}

bool RingBuffer_setSize(RingBuffer* r, unsigned int length, unsigned int datasize)
{
	r->datasize = datasize;
	
	if ( r->bufferlen == length )
		return true;
	
	if ( length % datasize != 0 )
		length += 4 - (length % datasize);
	
	void* buf = realloc(r->buffer, length);
	
	if ( buf == NULL )
		return 0;
	
	memset(buf, 0, length);
	r->buffer = buf;
	r->inpos = 0;
	r->outpos = 0;
	r->bufferlen = length;

	return false;
}

void RingBuffer_reset(RingBuffer* r)
{
	r->outpos = 0;
	r->inpos = 0;
//	r->fileOffset = 0;
}

unsigned int RingBuffer_getSize(RingBuffer* r)
{
	return r->bufferlen;
}

// XXX - This isn't quite right, should handle case where outpos = 0
unsigned int RingBuffer_getFreeSpace(RingBuffer* r)
{
	unsigned int o = r->outpos;
	unsigned int i = r->inpos;

	if ( i < o )
		return o - i - r->datasize;
	else
		return o - r->datasize + r->bufferlen - i;
}

/*
unsigned int RingBuffer_getOutputOffset(RingBuffer* r)
{
	return r->fileOffset + r->outpos;
}

void RingBuffer_setOutputOffset(RingBuffer* r, unsigned int offset)
{
	r->fileOffset = offset - r->outpos;
}
*/

unsigned int RingBuffer_getInputAvailableSize(RingBuffer* r)
{
	// save r->outpos in case another thread changes it
	unsigned int o = r->outpos;
	
	if ( o <= r->inpos )
		return r->bufferlen - r->inpos;
	else
		return o - r->inpos - r->datasize;
}

void* RingBuffer_getInputPointer(RingBuffer* r)
{
	return (uint8_t*)r->buffer + r->inpos;
}

void RingBuffer_moveInputPointer(RingBuffer* r, unsigned int bytes)
{
	if ( r->bufferlen == 0 )
		printf("what??\n");
	
	r->inpos = (r->inpos + bytes) % r->bufferlen;

	if ( r->inpos == r->bufferlen && r->outpos > 0 )
		r->inpos = 0;
}

#if !defined(MIN)
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif

unsigned int RingBuffer_addData(RingBuffer* r, const void* ptr, unsigned int length)
{
	int c = (r->inpos >= r->outpos) ? 2 : 1;
	unsigned int l = length;
	const uint8_t* data = ptr;
	
	while ( c-- > 0 && l > 0 )
	{
		unsigned int n = MIN(l, RingBuffer_getInputAvailableSize(r));
		memcpy(RingBuffer_getInputPointer(r), data, n);
		RingBuffer_moveInputPointer(r, n);
		data += n;
		l -= n;
	}
	
	return length - l;
}

unsigned int RingBuffer_getBytesAvailable(RingBuffer* r)
{
	// save r->inpos in case another thread changes it
	unsigned int i = r->inpos;
	unsigned int o = r->outpos;
	
	if ( o <= i )
		return i - o;
	else
		return r->bufferlen - o + i;
}

unsigned int RingBuffer_getOutputAvailableSize(RingBuffer* r)
{
	unsigned int i = r->inpos;
	
	if ( r->outpos <= i )
		return i - r->outpos;
	else
		return r->bufferlen - r->outpos;
}

void* RingBuffer_getOutputPointer(RingBuffer* r)
{
	return (uint8_t*)r->buffer + r->outpos;
}

void RingBuffer_moveOutputPointer(RingBuffer* r, unsigned int bytes)
{
	r->outpos = (r->outpos + bytes) % r->bufferlen;
	
	/* no! we can't touch inpos!
	if ( r->outpos == r->inpos )
	{
		r->outpos = r->inpos = 0;
		return;
	}
	*/
	// wrap inpos around if it's waiting for outpos to get off the 0 position
	if ( r->inpos == r->bufferlen && r->outpos != 0 )
		r->inpos = 0;
	
	if ( r->outpos == r->bufferlen && r->inpos != 0 )
	{
//		r->fileOffset += r->bufferlen;
		r->outpos = 0;
	}
}

unsigned int RingBuffer_readData(RingBuffer* r, void* ptr, unsigned int length)
{
//	printf("RingBuffer_readData(%p, %p, %i)\n", r, ptr, length);
	
	int c = (r->outpos > r->inpos) ? 2 : 1;
	unsigned int l = length;
	uint8_t* data = ptr;
	
	while ( c-- > 0 && l > 0 )
	{
		unsigned int n = MIN(l, RingBuffer_getOutputAvailableSize(r));
		memcpy(data, RingBuffer_getOutputPointer(r), n);
		RingBuffer_moveOutputPointer(r, n);
		data += n;
		l -= n;
	}
	
	return length - l;
}
