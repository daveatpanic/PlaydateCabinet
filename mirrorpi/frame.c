//
//  frame.c
//  MirrorJr
//
//  Created by Dave Hayden on 9/25/24.
//

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "frame.h"
#include "constants.h"

//#define LOG printf
#define LOG(s)

SDL_Renderer* renderer;
SDL_Texture* sdl_texture = NULL;
uint8_t* framebuffer1bit;
unsigned int* framebuffer32bit; // ARGB data

#define DISPLAY_BLACK 0xff000000
#define DISPLAY_WHITE 0xffb1afa8

enum FrameMode
{
	kFrame1bit,
	kFrame4bit_2x2,
} mode = kFrame1bit;

uint32_t palette[16] = {
	DISPLAY_BLACK, DISPLAY_WHITE
};

int render_w = LCD_COLUMNS;
int render_h = LCD_ROWS;

bool frame_init(SDL_Window* window)
{
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	if ( renderer == NULL )
	{
		printf("couldn't create renderer: %s\n", SDL_GetError());
		return 4;
	}

	SDL_RendererInfo info;
	
	if ( SDL_GetRendererInfo(renderer, &info) != 0 )
	{
		printf("SDL_GetRendererInfo failed: %s", SDL_GetError());
		return false;
	}

	if ( SDL_InitSubSystem(SDL_INIT_VIDEO) != 0 )
	{
		printf("video init failed: %s", SDL_GetError());
		return false;
	}
	
	framebuffer32bit = calloc(1, LCD_ROWS * LCD_COLUMNS * 4);
	framebuffer1bit = calloc(1, LCD_ROWSIZE * LCD_COLUMNS);

	assert(sdl_texture == NULL);

	sdl_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, render_w, render_h);
	assert(sdl_texture);
	SDL_SetTextureBlendMode(sdl_texture, SDL_BLENDMODE_BLEND);
	
	return true;
}

// input buffer is LCD_COLUMNS x LCD_ROWS with LCD_ROWSIZE bytes per row
// output buffer is render_w x render_h and render_w * 4 bytes per row

static void convertTo32Bit_1bit(const uint8_t* in, uint32_t* out)
{
	for ( int y = 0; y < LCD_ROWS; ++y )
	{
		for ( int x = 0; x < LCD_COLUMNS / 8; ++x )
		{
			unsigned char src_bits = in[x + y * LCD_ROWSIZE];

			for ( int bit = 0; bit < 8; bit++ )
			{
				unsigned char bitmask = 0x80 >> bit;
				unsigned int dest_color = (src_bits & bitmask) ? palette[1] : palette[0];
				int dest_x = (x * 8) + bit;
				int out_i = dest_x + y * render_w;
				out[out_i] = dest_color;
			}
		}
	}
}

static void convertTo32Bit_4bit_2x2(const uint8_t* in, uint32_t* out)
{
	for ( int y = 0; y < LCD_ROWS; y += 2 )
	{
		for ( int x = 0; x < LCD_COLUMNS / 8; ++x )
		{
			unsigned char src_bits1 = in[x + y * LCD_ROWSIZE];
			unsigned char src_bits2 = in[x + (y+1) * LCD_ROWSIZE];

			for ( int bit = 0; bit < 8; bit += 2 )
			{
				unsigned char bitmask1 = 0x80 >> bit;
				unsigned char bitmask2 = 0x80 >> (bit+1);
				
				unsigned int idx =
					((src_bits1 & bitmask1) ? 8 : 0) |
					((src_bits1 & bitmask2) ? 4 : 0) |
					((src_bits2 & bitmask1) ? 2 : 0) |
					((src_bits2 & bitmask2) ? 1 : 0);
				
				unsigned int dest_color = palette[idx];
				
				int dest_x = (x * 8) + bit;
				int out_i = dest_x + y * render_w;

				// instead of switching resolution, we'll draw 2x2
				out[out_i] = dest_color;
				out[out_i+1] = dest_color;
				out[out_i+render_w] = dest_color;
				out[out_i+render_w+1] = dest_color;
			}
		}
	}
}

void frame_reset()
{
	mode = kFrame1bit;
	palette[0] = DISPLAY_BLACK;
	palette[1] = DISPLAY_WHITE;
}

void frame_set1BitPalette(RGB rgb[2])
{
	mode = kFrame1bit;
	
	for ( int i = 0; i < 2; ++i )
		palette[i] = 0xff000000 | ((int)rgb[i].r << 16) | ((int)rgb[i].g << 8) | rgb[i].r;
}

void frame_set4BitPalette(RGB rgb[16])
{
	mode = kFrame4bit_2x2;
	
	for ( int i = 0; i < 16; ++i )
		palette[i] = 0xff000000 | ((uint32_t)rgb[i].r << 16) | ((uint32_t)rgb[i].g << 8) | rgb[i].b;
}

void frame_setMode(enum FrameMode inmode, uint32_t pal[16])
{
	mode = inmode;
	memcpy(&palette[0], &pal[0], sizeof(palette));
}

void frame_present()
{
	if ( mode == kFrame1bit )
		convertTo32Bit_1bit(framebuffer1bit, framebuffer32bit);
	else
		convertTo32Bit_4bit_2x2(framebuffer1bit, framebuffer32bit);

	SDL_UpdateTexture(sdl_texture, NULL, framebuffer32bit, render_w * 4);

	int rw, rh;
	SDL_GetRendererOutputSize(renderer, &rw, &rh);

	SDL_Rect src_rect = { 0, 0, render_w, render_h };
//	SDL_Rect dst_rect = { 0, 0, rw, rh };
	SDL_Rect dst_rect = { 40, 0, 1200, 720 }; // XXX don't hardcode

	SDL_RenderCopy(renderer, sdl_texture, &src_rect, &dst_rect);
	SDL_RenderPresent(renderer);
}

#include "pdimage.h"

const uint8_t dot[5] = { 0x00, 0x00, 0x00, 0x00, 0x02 };

const uint8_t digits[10][5] =
{
	{ 0x07, 0x05, 0x05, 0x05, 0x07 }, // 0
	{ 0x06, 0x02, 0x02, 0x02, 0x07 }, // 1
	{ 0x07, 0x01, 0x07, 0x04, 0x07 }, // 2
	{ 0x07, 0x01, 0x07, 0x01, 0x07 }, // 3
	{ 0x05, 0x05, 0x07, 0x01, 0x01 }, // 4
	{ 0x07, 0x04, 0x07, 0x01, 0x07 }, // 5
	{ 0x07, 0x04, 0x07, 0x05, 0x07 }, // 6
	{ 0x07, 0x01, 0x01, 0x01, 0x01 }, // 7
	{ 0x07, 0x05, 0x07, 0x05, 0x07 }, // 8
	{ 0x07, 0x05, 0x07, 0x01, 0x07 }, // 9
};

#include <netinet/in.h>

void frame_showWaitScreen(const uint8_t* buf)
{
	LOG("showWaitScreen()\n");
	memcpy(framebuffer1bit, image_dat, image_dat_len);
	
	// XXX fix in source data instead
	for ( int i = 0; i < LCD_ROWS * LCD_COLUMNS/8; ++i )
		framebuffer1bit[i] ^= 0xff;
	
	uint8_t* start = framebuffer1bit + (LCD_ROWS - 4) * LCD_COLUMNS/8 - (strlen((char*)buf)+1)/2;
	int lohi = 1;
	
	for ( int x = 0; x < INET_ADDRSTRLEN && buf[x] != 0; ++x )
	{
		uint8_t* p = start + x/2;
		const uint8_t* dat = (buf[x] == '.') ? dot : digits[buf[x]-'0'];
		
		for ( int y = 0; y < 5; ++y )
		{
			*p |= *dat << (lohi * 4);
			p += LCD_COLUMNS/8;
			dat += 1;
		}
		
		lohi ^= 1;
	}
	
	LOG("calling frame_present()\n");
	frame_present();
}

void frame_begin(uint32_t timestamp_ms)
{
	// we should delay to match the timestamp, but.. meh.
}

void frame_end()
{
	// post current frame?
	frame_present();
}

void frame_setRow(unsigned int rowNum, const uint8_t* row)
{
	//LOG("row %i\n", rowNum);
	memcpy(framebuffer1bit + (rowNum-1)*LCD_ROWSIZE, row, LCD_ROWSIZE);
}

