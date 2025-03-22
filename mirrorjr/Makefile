
#SRC = main.c audio.c controls.c frame.c ringbuffer.c serial.c stream.c
CC = gcc
OPT = -g -Os
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
#CFLAGS = $(shell sdl2-config --cflags) -fsanitize=address
CFLAGS = -Wall -Wsign-conversion $(shell sdl2-config --cflags)
#LIBS = $(shell sdl2-config --libs) -latomic -fsanitize=address -static-libasan
LIBS = $(shell sdl2-config --libs)

all: mirror

rpi: CFLAGS += -DTARGET_RPI
rpi: LIBS += -ludev -lpigpio
rpi: mirror

macos: CFLAGS += -DTARGET_MACOS
macos: LIBS += -framework Foundation -framework IOKit
macos: mirror

mirror: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o mirror

debug: CFLAGS += -DDEBUG
debug: rpi

%.o: %.c
	$(CC) -c $(OPT) -I . $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) mirror
