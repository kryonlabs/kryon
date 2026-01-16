# Codegens Common Utilities Makefile
# Builds shared utilities for all code generators

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../build
CJSON_DIR = ../third_party/cJSON
IR_DIR = ../ir

# React Common (for TSX/JSX codegens)
REACT_COMMON_OBJ = $(BUILD_DIR)/react_common.o
REACT_COMMON_SRC = react_common.c
REACT_COMMON_HDR = react_common.h

# Codegen Common (for all codegens)
CODEGEN_COMMON_OBJ = $(BUILD_DIR)/codegen_common.o
CODEGEN_COMMON_SRC = codegen_common.c
CODEGEN_COMMON_HDR = codegen_common.h

# Output
COMMON_LIB = $(BUILD_DIR)/libkryon_codegen_common.a

# Default target - build both common utilities
all: $(BUILD_DIR) $(REACT_COMMON_OBJ) $(CODEGEN_COMMON_OBJ) $(COMMON_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile react_common object file (create both local and build versions)
$(REACT_COMMON_OBJ): $(REACT_COMMON_SRC) $(REACT_COMMON_HDR)
	$(CC) $(CFLAGS) -I$(IR_DIR) -I$(CJSON_DIR) -c $(REACT_COMMON_SRC) -o react_common.o
	cp react_common.o $(REACT_COMMON_OBJ)

# Compile codegen_common object file
$(CODEGEN_COMMON_OBJ): $(CODEGEN_COMMON_SRC) $(CODEGEN_COMMON_HDR)
	$(CC) $(CFLAGS) -I$(IR_DIR) -I$(CJSON_DIR) -c $(CODEGEN_COMMON_SRC) -o $(BUILD_DIR)/codegen_common.o

# Create static library from codegen_common
$(COMMON_LIB): $(CODEGEN_COMMON_OBJ)
	$(AR) $(ARFLAGS) $@ $(CODEGEN_COMMON_OBJ)

# Also provide a local object file for tsx/jsx to depend on
react_common.o: $(REACT_COMMON_SRC) $(REACT_COMMON_HDR)
	$(CC) $(CFLAGS) -I$(IR_DIR) -I$(CJSON_DIR) -c $(REACT_COMMON_SRC) -o react_common.o
	cp react_common.o $(BUILD_DIR)/

# Clean
clean:
	rm -f react_common.o $(REACT_COMMON_OBJ) $(CODEGEN_COMMON_OBJ) $(COMMON_LIB)

# Install headers
install: all
	cp $(REACT_COMMON_HDR) $(BUILD_DIR)/
	cp $(CODEGEN_COMMON_HDR) $(BUILD_DIR)/

.PHONY: all clean install
