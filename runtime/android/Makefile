# Android Backend Makefile

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c11 -D__ANDROID__ -I. -I../../core/include -I../../ir -I../../renderers/android -I../../platforms/android
LDFLAGS =

SOURCES = ir_android_renderer.c android_rendering.c android_layout.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = libkryon_android_backend.a

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	@echo "Android backend built successfully"
