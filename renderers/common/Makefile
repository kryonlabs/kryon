# Makefile for common renderer utilities
CC ?= gcc
CFLAGS = -Wall -Wextra -std=c99 -O2 -fPIC -DKRYON_TARGET_PLATFORM=0
INCLUDES = -I../../core/include -I../../ir/include
LIBS =
BUILD_DIR = ../../build

# Source files
SOURCES = command_buf.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = $(BUILD_DIR)/libkryon_command_buf.a

# Default target
all: $(TARGET) install

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
	@echo "Library already in build directory"

.PHONY: all clean install
