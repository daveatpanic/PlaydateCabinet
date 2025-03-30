//
//  frame.h
//  MirrorJr
//
//  Created by Dave Hayden on 9/25/24.
//

#ifndef frame_h
#define frame_h

#include <stdbool.h>
#include "SDL.h"

#define LCD_ROWS 240
#define LCD_COLUMNS 400
#define LCD_ROWSIZE (LCD_COLUMNS/8)

bool frame_init(SDL_Window* window);
void frame_begin(uint32_t timestamp);
void frame_setRow(unsigned int row, const uint8_t* data);
void frame_end();
void frame_present();
void frame_showWaitScreen(const uint8_t* addr);

void frame_reset();

typedef struct { uint8_t r; uint8_t g; uint8_t b; } RGB;

void frame_set1BitPalette(RGB palette[2]);
void frame_set4BitPalette(RGB palette[16]);

#endif /* frame_h */
