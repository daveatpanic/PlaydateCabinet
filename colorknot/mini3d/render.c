//
//  render.c
//  Extension
//
//  Created by Dave Hayden on 10/20/15.
//  Copyright © 2015 Panic, Inc. All rights reserved.
//

#include <stdint.h>
#include <string.h>
#include "render.h"

#define LCD_ROWS 120
#define LCD_COLUMNS 200

#if !defined(MIN)
#define MIN(a, b) (((a)<(b))?(a):(b))
#endif

#if !defined(MAX)
#define MAX(a, b) (((a)>(b))?(a):(b))
#endif

#if ENABLE_Z_BUFFER
static uint16_t zbuf[LCD_COLUMNS*LCD_ROWS];
static float zscale;

#define Z_BIAS 0

void resetZBuffer(float zmin)
{
	memset(zbuf, 0, sizeof(zbuf));
	zscale = 0xffff * (zmin + Z_BIAS);
}
#endif

static inline uint32_t swap(uint32_t n)
{
#if TARGET_PLAYDATE
	//return __REV(n);
	uint32_t result;
	
	__asm volatile ("rev %0, %1" : "=l" (result) : "l" (n));
	return(result);
#else
	return ((n & 0xff000000) >> 24) | ((n & 0xff0000) >> 8) | ((n & 0xff00) << 8) | (n << 24);
#endif
}

/*
#if ENABLE_Z_BUFFER
void getZMask(uint8_t* buffer, int rowstride)
{
	for ( int y = 0; y < LCD_ROWS; ++y )
	{
		uint32_t* row = (uint32_t*)&buffer[y*rowstride];
		uint16_t* zrow = &zbuf[y*LCD_COLUMNS];
		uint32_t mask = 0;
		int x = 0;
		
		while ( x < rowstride )
		{
			if ( zrow[x] < 0xffff )
				mask |= 0x80000000 >> (x%32);
			
			++x;
			
			if ( x%32 == 0 )
			{
				*row++ = swap(mask);
				mask = 0;
			}
		}
		
		if ( x%32 != 0 )
			*row++ = swap(mask);
	}
}
#endif
*/

static void
drawFragment(uint8_t* row, int x1, int x2, uint8_t color)
{
	memset(row+x1, color, x2-x1);
}

#if ENABLE_Z_BUFFER
static void
drawFragment_zbuf(uint8_t* row, uint16_t* zbrow, int x, int endx, uint32_t z, int32_t dzdx, uint8_t color)
{
	if ( endx < 0 || x >= LCD_COLUMNS )
		return;
	
	if ( x < 0 )
	{
		z += -x * dzdx;
		x = 0;
	}
	
	if ( endx > LCD_COLUMNS )
		endx = LCD_COLUMNS;

	while ( x < endx )
	{
		uint16_t zi = z >> 16;
		
		if ( zi > zbrow[x] )
		{
			row[x] = color;
			zbrow[x] = zi;
		}
		
		++x;
		z += dzdx;
	}
}
#endif

static inline int32_t slope(float x1, float y1, float x2, float y2)
{
	float dx = x2-x1;
	float dy = y2-y1;
	
	if ( dy < 1 )
		return dx * (1<<16);
	else
		return dx / dy * (1<<16);
}

#if ENABLE_Z_BUFFER
LCDRowRange
drawLine_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t color)
{
	if ( p1->y > p2->y )
	{
		Point3D* tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	int y = p1->y;
	int endy = p2->y;
	
	if ( y >= LCD_ROWS || endy < 0 || MIN(p1->x, p2->x) >= LCD_COLUMNS || MAX(p1->x, p2->x) < 0 )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y + 1);
	float py = p1->y;

	// move lines a bit forward so they don't get buried in solid geometry
	float z1 = zscale / (p1->z + Z_BIAS) + 256;
	float z2 = zscale / (p2->z + Z_BIAS) + 256;

	if ( z1 > 65535 ) z1 = 65535;
	if ( z2 > 65535 ) z2 = 65535;

	int32_t dzdy = slope(z1, p1->y, z2, p2->y + 1);
	int32_t dzdx = slope(z1, p1->x, z2, p2->x);

	uint32_t z = z1 * (1<<16);

	if ( y < 0 )
	{
		x += -y * dx;
		z += -y * dzdy;
		y = 0;
		py = 0;
	}
	
	int32_t x1 = x + dx * (y+1-py);

	while ( y <= endy )
	{
		if ( y == endy )
			x1 = p2->x * (1<<16);

		if ( dx < 0 )
		{
			z += dzdy;
			drawFragment_zbuf(&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], x1>>16, (x>>16) + thick, z, dzdx, color);
		}
		else
		{
			drawFragment_zbuf(&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], x>>16, (x1>>16) + thick, z, dzdx, color);
			z += dzdy;
		}

		if ( ++y == LCD_ROWS )
			break;
		
		x = x1;
		x1 += dx;
	}
	
	return (LCDRowRange){ MAX(0, p1->y), MIN(LCD_ROWS, p2->y) };
}
#endif

LCDRowRange
drawLine(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, int thick, uint8_t color)
{
	if ( p1->y > p2->y )
	{
		Point3D* tmp = p1;
		p1 = p2;
		p2 = tmp;
	}

	int y = p1->y;
	int endy = p2->y;
	
	if ( y >= LCD_ROWS || endy < 0 || MIN(p1->x, p2->x) >= LCD_COLUMNS || MAX(p1->x, p2->x) < 0 )
		return (LCDRowRange){ 0, 0 };
	
	int32_t x = p1->x * (1<<16);
	int32_t dx = slope(p1->x, p1->y, p2->x, p2->y);
	float py = p1->y;
	
	if ( y < 0 )
	{
		x += -p1->y * dx;
		y = 0;
		py = 0;
	}

	int32_t x1 = x + dx * (y+1-py);

	while ( y <= endy )
	{
		if ( y == endy )
			x1 = p2->x * (1<<16);
		
		if ( dx < 0 )
			drawFragment(&bitmap[y*rowstride], x1>>16, (x>>16) + thick, color);
		else
			drawFragment(&bitmap[y*rowstride], x>>16, (x1>>16) + thick, color);
		
		if ( ++y == LCD_ROWS )
			break;

		x = x1;
		x1 += dx;
	}
	
	return (LCDRowRange){ MAX(0, p1->y), MIN(LCD_ROWS, p2->y) };
}

static void fillRange(uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2, uint8_t color)
{
	int32_t x1 = *x1p, x2 = *x2p;
	
	if ( endy < 0 )
	{
		int dy = endy - y;
		*x1p = x1 + dy * dx1;
		*x2p = x2 + dy * dx2;
		return;
	}
	
	if ( y < 0 )
	{
		x1 += -y * dx1;
		x2 += -y * dx2;
		y = 0;
	}
	
	while ( y < endy )
	{
		drawFragment(&bitmap[y*rowstride], (x1>>16), (x2>>16)+1, color);
		
		x1 += dx1;
		x2 += dx2;
		++y;
	}
	
	*x1p = x1;
	*x2p = x2;
}

#if ENABLE_Z_BUFFER
static void fillRange_zbuf(uint8_t* bitmap, int rowstride, int y, int endy, int32_t* x1p, int32_t dx1, int32_t* x2p, int32_t dx2, uint32_t* zp, int32_t dzdy, int32_t dzdx, uint8_t color)
{
	int32_t x1 = *x1p, x2 = *x2p;
	uint32_t z = *zp;
	
	if ( endy < 0 )
	{
		int dy = endy - y;
		*x1p = x1 + dy * dx1;
		*x2p = x2 + dy * dx2;
		*zp = z + dy * dzdy;
		return;
	}

	if ( y < 0 )
	{
		x1 += -y * dx1;
		x2 += -y * dx2;
		z += -y * dzdy;
		y = 0;
	}

	while ( y < endy )
	{
		drawFragment_zbuf(&bitmap[y*rowstride], &zbuf[y*LCD_COLUMNS], (x1>>16), (x2>>16)+1, z, dzdx, color);
		
		x1 += dx1;
		x2 += dx2;
		z += dzdy;
		++y;
	}
	
	*x1p = x1;
	*x2p = x2;
	*zp = z;
}
#endif

static inline void sortTri(Point3D** p1, Point3D** p2, Point3D** p3)
{
	float y1 = (*p1)->y, y2 = (*p2)->y, y3 = (*p3)->y;
	
	if ( y1 <= y2 && y1 < y3 )
	{
		if ( y3 < y2 ) // 1,3,2
		{
			Point3D* tmp = *p2;
			*p2 = *p3;
			*p3 = tmp;
		}
	}
	else if ( y2 < y1 && y2 < y3 )
	{
		Point3D* tmp = *p1;
		*p1 = *p2;

		if ( y3 < y1 ) // 2,3,1
		{
			*p2 = *p3;
			*p3 = tmp;
		}
		else // 2,1,3
			*p2 = tmp;
	}
	else
	{
		Point3D* tmp = *p1;
		*p1 = *p3;
		
		if ( y1 < y2 ) // 3,1,2
		{
			*p3 = *p2;
			*p2 = tmp;
		}
		else // 3,2,1
			*p3 = tmp;
	}

}

LCDRowRange fillTriangle(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t color)
{
	// sort by y coord
	
	sortTri(&p1, &p2, &p3);
	
	int endy = MIN(LCD_ROWS, p3->y);
	
	if ( p1->y > LCD_ROWS || endy < 0 )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;
	
	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

	int32_t dx1 = MIN(sb, sc);
	int32_t dx2 = MAX(sb, sc);
	
	fillRange(bitmap, rowstride, p1->y, MIN(LCD_ROWS, p2->y), &x1, dx1, &x2, dx2, color);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y);
	
	if ( sb < sc )
	{
		x1 = p2->x * (1<<16);
		fillRange(bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, color);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange(bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, color);
	}
	
	return (LCDRowRange){ MAX(0, p1->y), endy };
}

#if ENABLE_Z_BUFFER
LCDRowRange fillTriangle_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, uint8_t color)
{
	sortTri(&p1, &p2, &p3);
	
	int endy = MIN(LCD_ROWS, p3->y);
	int det = (p3->x - p1->x) * (p2->y - p1->y) - (p2->x - p1->x) * (p3->y - p1->y);
	
	if ( p1->y > LCD_ROWS || endy < 0 || det == 0 )
		return (LCDRowRange){ 0, 0 };

	int32_t x1 = p1->x * (1<<16);
	int32_t x2 = x1;

	int32_t sb = slope(p1->x, p1->y, p2->x, p2->y);
	int32_t sc = slope(p1->x, p1->y, p3->x, p3->y);

	int32_t dx1 = MIN(sb,sc);
	int32_t dx2 = MAX(sb,sc);
	
	float z1 = zscale / (p1->z + Z_BIAS);
	float z2 = zscale / (p2->z + Z_BIAS);
	float z3 = zscale / (p3->z + Z_BIAS);

	float mx = p1->x + (p2->y-p1->y) * (p3->x-p1->x) / (p3->y-p1->y);
	float mz = z1 + (p2->y-p1->y) * (z3-z1) / (p3->y-p1->y);

	int32_t dzdx, dzdy;

	if ( sc < sb )
	{
		dzdx = slope(mz, mx, z2, p2->x);
		dzdy = slope(z1, p1->y, z3, p3->y);
	}
	else
	{
		dzdx = slope(z2, p2->x, mz, mx);
		dzdy = slope(z1, p1->y, z2, p2->y);
	}
	
	uint32_t z = z1 * (1<<16);

	fillRange_zbuf(bitmap, rowstride, p1->y, MIN(LCD_ROWS, p2->y), &x1, dx1, &x2, dx2, &z, dzdy, dzdx, color);
	
	int dx = slope(p2->x, p2->y, p3->x, p3->y);

	if ( sb < sc )
	{
		dzdy = slope(z2, p2->y, z3, p3->y);
		x1 = p2->x * (1<<16);
		z = z2 * (1<<16);
		fillRange_zbuf(bitmap, rowstride, p2->y, endy, &x1, dx, &x2, dx2, &z, dzdy, dzdx, color);
	}
	else
	{
		x2 = p2->x * (1<<16);
		fillRange_zbuf(bitmap, rowstride, p2->y, endy, &x1, dx1, &x2, dx, &z, dzdy, dzdx, color);
	}
	
	return (LCDRowRange){ MAX(0, p1->y), endy };
}
#endif

LCDRowRange fillQuad(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t color)
{
	// XXX - implement with 3 fillRange_zbuf() calls
	fillTriangle(bitmap, rowstride, p1, p2, p3, color);
	return fillTriangle(bitmap, rowstride, p1, p3, p4, color);
}

#if ENABLE_Z_BUFFER
LCDRowRange fillQuad_zbuf(uint8_t* bitmap, int rowstride, Point3D* p1, Point3D* p2, Point3D* p3, Point3D* p4, uint8_t color)
{
	// XXX - implement with 3 fillRange_zbuf() calls
	fillTriangle_zbuf(bitmap, rowstride, p1, p2, p3, color);
	return fillTriangle_zbuf(bitmap, rowstride, p1, p3, p4, color);
}
#endif
