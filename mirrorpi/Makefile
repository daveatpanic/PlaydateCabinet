
#SRC = main.c audio.c controls.c frame.c ringbuffer.c serial.c stream.c
CC = gcc
OPT = -g -Os
SRC = $(wildcard *.c)
OBJS = $(SRC:.c=.o)
NO_CONTROLS_SRC = $(filter-out controls.c,$(SRC))
NO_CONTROLS_OBJS = $(NO_CONTROLS_SRC:.c=.no-controls.o)
#CFLAGS = $(shell sdl2-config --cflags) -fsanitize=address
CFLAGS = -Wall -Wsign-conversion $(shell sdl2-config --cflags)
#LIBS = $(shell sdl2-config --libs) -latomic -fsanitize=address -static-libasan
LIBS = $(shell sdl2-config --libs)

all: mirror

rpi: CFLAGS += -DTARGET_RPI
rpi: LIBS += -ludev -lpigpio
rpi: mirror

rpi-no-controls: CFLAGS += -DTARGET_RPI -DMIRRORPI_NO_CONTROLS
rpi-no-controls: LIBS += -ludev
rpi-no-controls: mirror-no-controls

macos: CFLAGS += -DTARGET_MACOS
macos: LIBS += -framework Foundation -framework IOKit
macos: mirror

mirror: $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o mirror

mirror-no-controls: $(NO_CONTROLS_OBJS)
	$(CC) $(NO_CONTROLS_OBJS) $(LIBS) -o mirror-no-controls

debug: CFLAGS += -DDEBUG
debug: rpi

%.o: %.c
	$(CC) -c $(OPT) -I . $(CFLAGS) $< -o $@

%.no-controls.o: %.c
	$(CC) -c $(OPT) -I . $(CFLAGS) $< -o $@

clean:
	rm -f $(OBJS) $(NO_CONTROLS_OBJS) mirror mirror-no-controls
