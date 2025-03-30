//
//  controls.c
//  MirrorJr
//
//  Created by Dave Hayden on 9/28/24.
//

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include "controls.h"
#include "stream.h"

#if TARGET_RPI
#include <pigpio.h>
#endif

// buttons are on the following GPIOs, crank is handled by an external
// microcontroller which sends us movement data over /dev/ttyS0

enum buttons
{
	kButtonUp,
	kButtonDown,
	kButtonLeft,
	kButtonRight,
	kButtonB,
	kButtonA,
	kButtonMenu
};

int pi = 0;
const unsigned int gpios[] = { 4, 27, 22, 23, 24, 25, 5 };
unsigned int crank = 0;

bool controls_init()
{
	int res = gpioInitialise();

	if ( res < 0 )
	{
		printf("gpioInitialise failed\n");
		return false;
	}

	int dev;
	
	if ( (dev = serOpen("/dev/ttyS0", 115200, 0)) < 0 )
	{
		printf("crank init failed\n");
		return false;
	}
		
	crank = (unsigned)dev;
	
	if ( gpioSetMode(4, PI_INPUT) < 0 || gpioSetPullUpDown(4, PI_PUD_UP) ||
		 gpioSetMode(27, PI_INPUT) < 0 || gpioSetPullUpDown(27, PI_PUD_UP) ||
		 gpioSetMode(22, PI_INPUT) < 0 || gpioSetPullUpDown(22, PI_PUD_UP) ||
		 gpioSetMode(23, PI_INPUT) < 0 || gpioSetPullUpDown(23, PI_PUD_UP) ||
		 gpioSetMode(24, PI_INPUT) < 0 || gpioSetPullUpDown(24, PI_PUD_UP) ||
		 gpioSetMode(25, PI_INPUT) < 0 || gpioSetPullUpDown(25, PI_PUD_UP) ||
		 gpioSetMode(5, PI_INPUT) < 0 || gpioSetPullUpDown(6, PI_PUD_UP) )
	{
		printf("gpioSetMode/PullUpDown failed\n");
		return false;
	}

	return true;
}

int buttonon[7] = {0};

void controls_scan()
{
	// XXX - debounce if needed
	
	for ( int i = 0; i < 7; ++i )
	{
		int on = 1-gpioRead(gpios[i]);
		
		if ( on && !buttonon[i] )
			stream_sendButtonPress(i);
		else if ( !on && buttonon[i] )
			stream_sendButtonRelease(i);
		
		buttonon[i] = on;
	}

	static char readbuf[6] = {0};
	static int readpos = 0;
	static float lastangle = -1;
	float crankangle = -1;
	
	// rate limit the crank
	static int limit = 0;
	
	if ( ++limit < 3 )
		return;
	
	limit = 0;

	while ( serDataAvailable(crank) )
	{
		int b = serReadByte(crank);
		
		if ( b == '\n' )
		{
			if ( isdigit(readbuf[0]) )
				crankangle = atof(readbuf);
			else if ( readpos == 4 && strncmp(readbuf, "out", 3) == 0 )
				stream_sendCrankDocked(true);
			else if ( readpos == 3 && strncmp(readbuf, "in", 2) == 0 )
				stream_sendCrankDocked(false);

			//printf("read %f", crankangle);
			readpos = 0;
		}
		else if ( readpos < 5 )
			readbuf[readpos++] = b;
	}
	
	if ( crankangle != -1 )
	{
		if ( lastangle != -1 )
		{
			float change = crankangle - lastangle;
			
			if ( change > 180 ) change -= 360;
			else if ( change < -180 ) change += 360;
			
			if ( change != 0 )
				stream_sendCrankChange(change);
			
			// printf("sending crank change %f\n", change);
		}
		
		lastangle = crankangle;
	}
}
