CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -std=c11

.PHONY: all clean

all: otfchop

otfchop: otfchop.c stb_truetype.h stb_image_write.h
	$(CC) $(CFLAGS) otfchop.c -lm -o otfchop

clean:
	rm -f otfchop
