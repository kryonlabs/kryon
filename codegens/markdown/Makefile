# Markdown Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../build
CJSON_DIR = ../../ir/third_party/cJSON

# Output
LIB_NAME = libkryon_codegen_markdown
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# Sources
SOURCES = markdown_codegen.c
OBJECTS = markdown_codegen.o
HEADERS = markdown_codegen.h

# Default target
all: $(BUILD_DIR) $(STATIC_LIB) $(SHARED_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object file
markdown_codegen.o: markdown_codegen.c markdown_codegen.h
	$(CC) $(CFLAGS) -I../../ir -I$(CJSON_DIR) -c markdown_codegen.c -o markdown_codegen.o

# Create static library
$(STATIC_LIB): markdown_codegen.o
	$(AR) $(ARFLAGS) $@ markdown_codegen.o

# Create shared library
$(SHARED_LIB): markdown_codegen.o
	$(CC) $(CFLAGS) -shared -o $@ markdown_codegen.o

# Clean
clean:
	rm -f *.o $(STATIC_LIB) $(SHARED_LIB)

# Install
install: all
	cp $(HEADERS) $(BUILD_DIR)/

.PHONY: all clean install
