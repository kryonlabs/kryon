# Nim Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../ir/third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_nim
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = nim_codegen.c
OBJECTS = nim_codegen.o
HEADERS = nim_codegen.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object file
nim_codegen.o: nim_codegen.c nim_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -c nim_codegen.c -o nim_codegen.o

# Create static library
$(STATIC_LIB): nim_codegen.o
	$(AR) $(ARFLAGS) $@ nim_codegen.o

# Create shared library
$(SHARED_LIB): nim_codegen.o
	$(CC) $(CFLAGS) -shared -o $@ nim_codegen.o

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
