# Lua Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_lua
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = lua_codegen.c
OBJECTS = lua_codegen.o
HEADERS = lua_codegen.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object file
lua_codegen.o: lua_codegen.c lua_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -c lua_codegen.c -o lua_codegen.o

# Create static library
$(STATIC_LIB): lua_codegen.o
	$(AR) $(ARFLAGS) $@ lua_codegen.o

# Create shared library
$(SHARED_LIB): lua_codegen.o
	$(CC) $(CFLAGS) -shared -o $@ lua_codegen.o

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
