# Nim Parser Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../../build
IR_DIR = ../..
CJSON_DIR = $(IR_DIR)/third_party/cJSON

# Output
LIB_NAME = libkryon_parser_nim
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = ir_nim_parser.c nim_lexer.c
OBJECTS = ir_nim_parser.o nim_lexer.o
HEADERS = ir_nim_parser.h nim_lexer.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files
ir_nim_parser.o: ir_nim_parser.c ir_nim_parser.h nim_lexer.h
	$(CC) $(CFLAGS) -I$(IR_DIR) -I$(CJSON_DIR) -c ir_nim_parser.c -o ir_nim_parser.o

nim_lexer.o: nim_lexer.c nim_lexer.h
	$(CC) $(CFLAGS) -c nim_lexer.c -o nim_lexer.o

# Create static library
$(STATIC_LIB): $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $(OBJECTS)

# Create shared library
$(SHARED_LIB): $(OBJECTS)
	$(CC) $(CFLAGS) -shared -o $@ $(OBJECTS)

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
