# TSX Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_tsx
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = tsx_codegen.c
OBJECTS = tsx_codegen.o
HEADERS = tsx_codegen.h

# Dependencies (react_common)
REACT_COMMON_DIR = ..
REACT_COMMON_OBJ = $(BUILD_DIR)/react_common.o

# Default target
all: $(BUILD_DIR) $(REACT_COMMON_OBJ) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Ensure react_common is built
$(REACT_COMMON_OBJ):
	cd $(REACT_COMMON_DIR) && $(MAKE) react_common.o

# Compile object file
tsx_codegen.o: tsx_codegen.c tsx_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -I$(REACT_COMMON_DIR) -c tsx_codegen.c -o tsx_codegen.o

# Create static library
$(STATIC_LIB): tsx_codegen.o $(REACT_COMMON_OBJ)
	$(AR) $(ARFLAGS) $@ tsx_codegen.o $(REACT_COMMON_OBJ)

# Create shared library
$(SHARED_LIB): tsx_codegen.o $(REACT_COMMON_OBJ)
	$(CC) $(CFLAGS) -shared -o $@ tsx_codegen.o $(REACT_COMMON_OBJ)

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
