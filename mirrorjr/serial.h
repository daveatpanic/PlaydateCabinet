//
//  serial.h
//  MirrorJr
//
//  Created by Dave Hayden on 9/28/24.
//

#ifndef serial_h
#define serial_h

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

bool ser_open();
bool ser_isOpen();
void ser_close();

ssize_t ser_write(const char* buffer, size_t size);
#define ser_writestr(s) ser_write(s, strlen(s));
ssize_t ser_writeLine(const char* cmd);

ssize_t ser_writeNonblocking(const char* buffer, size_t size);
ssize_t ser_read(uint8_t* buffer, size_t size);
void ser_flush(void);

#endif /* serial_h */
