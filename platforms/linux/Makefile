# Makefile for Linux Platform Integration Kit

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -fPIC
INCLUDES = -I../../core/include
LIBS = -L../../build -lkryon_core

# Source files
SOURCES = linux_platform.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = libkryon_linux_platform.a

# Conditional SDL3 support
SDL3_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null || echo "")
SDL3_LIBS = $(shell pkg-config --libs sdl3 2>/dev/null || echo "")

ifneq ($(SDL3_CFLAGS),)
    CFLAGS += $(SDL3_CFLAGS) -DKRYON_HAS_SDL3
    LIBS += $(SDL3_LIBS)
else
    CFLAGS += -Wno-unused
endif

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Creating static library: $(TARGET)"
	ar rcs $(TARGET) $(OBJECTS)

%.o: %.c
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	@echo "Installing library to ../../build/"
	cp $(TARGET) ../../build/

.PHONY: all clean install

# Test target
test: $(TARGET)
	@echo "Testing Linux platform integration..."
	$(CC) $(CFLAGS) $(INCLUDES) -o test_linux_platform test_linux_platform.c $(TARGET) $(LIBS) -lm
	./test_linux_platform

# Info target
info:
	@echo "Linux Platform Integration Kit"
	@echo "==============================="
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "SDL3 Support: $(if $(SDL3_CFLAGS),Yes,No)"
	@echo "Target: $(TARGET)"