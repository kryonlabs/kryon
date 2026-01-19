# C Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_c
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = ir_c_codegen.c
OBJECTS = ir_c_codegen.o
HEADERS = ir_c_codegen.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object file
ir_c_codegen.o: ir_c_codegen.c ir_c_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -c ir_c_codegen.c -o ir_c_codegen.o

# Create static library
$(STATIC_LIB): ir_c_codegen.o
	$(AR) $(ARFLAGS) $@ ir_c_codegen.o

# Create shared library
$(SHARED_LIB): ir_c_codegen.o
	$(CC) $(CFLAGS) -shared -o $@ ir_c_codegen.o

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
