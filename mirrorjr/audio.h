//
//  audio.h
//  MirrorJr
//
//  Created by Dave Hayden on 9/25/24.
//

#ifndef audio_h
#define audio_h

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

bool audio_init();
void audio_setFormat(unsigned int channels);

void audio_stop();

void audio_addData(uint8_t* data, unsigned int len);
void audio_addSilence(unsigned int len);

#endif /* audio_h */
