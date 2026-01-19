# Kry Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_kry
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = kry_codegen.c
OBJECTS = kry_codegen.o
HEADERS = kry_codegen.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object file
kry_codegen.o: kry_codegen.c kry_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -c kry_codegen.c -o kry_codegen.o

# Create static library
$(STATIC_LIB): kry_codegen.o
	$(AR) $(ARFLAGS) $@ kry_codegen.o

# Create shared library
$(SHARED_LIB): kry_codegen.o
	$(CC) $(CFLAGS) -shared -o $@ kry_codegen.o

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
