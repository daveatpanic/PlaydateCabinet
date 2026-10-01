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
#include "config.h"
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
	kButtonMenu,
	kButtonDock,
};

#if TARGET_RPI
struct config gpio;
unsigned int crankdev = 0;

int crankpos = 0;
int lastcrank = 0;
bool docked = true;

#define DEBOUNCE_TIME 100
int a_high = 0;
int b_high = 0;

void aFunction(int n, int level, uint32_t tick)
{
	//printf("A:%i\n", level);
	/*
	0 = change to low (a falling edge)
	1 = change to high (a rising edge)
	2 = no level change (a watchdog timeout)
	*/
	
	if ( level == 1 )
	{
		crankpos += b_high ? gpio.rotarymult : -gpio.rotarymult;
		a_high = 1;
	}
	else if ( level == 0 )
	{
		crankpos += b_high ? -gpio.rotarymult : gpio.rotarymult;
		a_high = 0;
	}
	
	crankpos = (crankpos+360)%360;
}

void bFunction(int n, int level, uint32_t tick)
{
	//printf("B:%i\n", level);
	
	if ( level == 1 )
	{
		crankpos += a_high ? -gpio.rotarymult : gpio.rotarymult;
		b_high = 1;
	}
	else if ( level == 0 )
	{
		crankpos += a_high ? gpio.rotarymult : -gpio.rotarymult;
		b_high = 0;
	}
	
	crankpos = (crankpos+360)%360;
}

bool initpin(int pin)
{
	return gpioSetMode((unsigned int)pin, PI_INPUT) == 0 &&
		gpioSetPullUpDown((unsigned int)pin, PI_PUD_UP) == 0 &&
		gpioGlitchFilter((unsigned int)pin, DEBOUNCE_TIME) == 0;
}
#endif

bool controls_init()
{
#if TARGET_RPI
	int res = gpioInitialise();
	get_config(&gpio);

	if ( res < 0 )
	{
		printf("gpioInitialise failed\n");
		return false;
	}

	if ( gpio.rotary1 != -1 && gpio.rotary2 != -1 )
	{
		if ( !initpin(gpio.rotary1) || !initpin(gpio.rotary2) )
		{
			printf("gpioSetMode/PullUpDown failed\n");
			return false;
		}
		
		gpioSetAlertFunc((unsigned int)gpio.rotary1, aFunction);
		gpioSetAlertFunc((unsigned int)gpio.rotary2, bFunction);
	}
	else
	{
		int dev;
		
		if ( (dev = serOpen("/dev/ttyS0", 115200, 0)) < 0 )
		{
			printf("crank init failed\n");
			return false;
		}
			
		crankdev = (unsigned)dev;
	}

	if ( gpio.dock != -1 && !initpin(gpio.dock) )
	{
		printf("gpioSetMode/PullUpDown failed\n");
		return false;
	}

	if ( !initpin(gpio.up) || !initpin(gpio.down) || !initpin(gpio.left) || !initpin(gpio.right) || 
		 !initpin(gpio.b) || !initpin(gpio.a) || !initpin(gpio.menu) )
	{
		printf("gpioSetMode/PullUpDown failed\n");
		return false;
	}
#endif

	return true;
}

int buttonon[8] = {0};

void controls_scan()
{
#if TARGET_RPI
	// XXX - debounce if needed
	
	int* gpios = (int*)&gpio;
	
	for ( int i = 0; i < 7; ++i )
	{
		int on = 1-(int)gpioRead((unsigned int)gpios[i]);
		
		if ( on && !buttonon[i] )
			stream_sendButtonPress(i);
		else if ( !on && buttonon[i] )
			stream_sendButtonRelease(i);
		
		buttonon[i] = on;
	}
	
	float crankangle = -1;
	static float lastangle = -1;
	
	if ( gpio.dock != -1 )
	{
		int on = 1-(int)gpioRead((unsigned int)gpios[kButtonDock]);
		
		if ( on && !buttonon[kButtonDock] )
		{
			docked = !docked;
			stream_sendCrankDocked(docked);

			if ( docked )
			{
				// make sure movement doesn't immediately undock
				crankpos = lastcrank;
				crankangle = lastangle;
			}
		}

		buttonon[kButtonDock] = on;
	}

	if ( gpio.rotary1 != -1 && gpio.rotary2 != -1 )
	{
		if ( docked && crankpos != lastcrank )
		{
			docked = false;
			stream_sendCrankDocked(false);
		}
		
		lastcrank = crankpos;
		crankangle = crankpos;
	}
	else
	{
		static char readbuf[6] = {0};
		static int readpos = 0;
		
		// rate limit the crank
		static int limit = 0;
		
		if ( ++limit < 3 )
			return;
		
		limit = 0;

		while ( serDataAvailable(crankdev) )
		{
			int b = serReadByte(crankdev);
			
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
	}

	if ( crankangle != -1 )
	{
		if ( lastangle != -1 )
		{
			float change = crankangle - lastangle;
			
			if ( change > 180 ) change -= 360;
			else if ( change < -180 ) change += 360;
			
			if ( change != 0 )
			{
				stream_sendCrankChange(change);
				//printf("sending crank change %f\n", change);
			}
		}
		
		lastangle = crankangle;
	}
#endif
}
