# Makefile for common renderer utilities
CC = clang
CFLAGS = -Wall -Wextra -std=c99 -O2 -DKRYON_PLATFORM_DESKTOP
INCLUDES = -I../../core/include
LIBS =

# Source files
SOURCES = command_buf.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = libkryon_common.a

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
