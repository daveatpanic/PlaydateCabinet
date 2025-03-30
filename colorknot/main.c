//
//  main.c
//  Extension
//
//  Created by Dave Hayden on 7/30/14.
//  Copyright (c) 2014 Panic, Inc. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>

#include "pd_api.h"
#include "luaglue.h"

#ifdef _WINDLL
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI* playdate, PDSystemEvent event, uint32_t arg)
{
	if ( event == kEventInitLua )
		register3D(playdate);
	else if ( event == kEventStreamStarted )
	{
		uint8_t palette[3*16] = {
			0x00, 0x00, 0x00,
			0x11, 0x11, 0x11,
			0x22, 0x22, 0x22,
			0x33, 0x33, 0x33,
			0x44, 0x44, 0x44,
			0x55, 0x55, 0x55,
			0x66, 0x66, 0x66,
			0x77, 0x77, 0x77,
			0x88, 0x88, 0x88,
			0x99, 0x99, 0x99,
			0xaa, 0xaa, 0xaa,
			0xbb, 0xbb, 0xbb,
			0xcc, 0xcc, 0xcc,
			0xdd, 0xdd, 0xdd,
			0xee, 0xee, 0xee,
			0xff, 0xff, 0xff,
		};
		
		playdate->system->sendMirrorData(2, palette, 3*16);
	}

	return 0;
}
