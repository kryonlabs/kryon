# React Common Utilities Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../build
CJSON_DIR = ../ir/third_party/cJSON

# Output
REACT_COMMON_OBJ = $(BUILD_DIR)/react_common.o
REACT_COMMON_SRC = react_common.c
REACT_COMMON_HDR = react_common.h

# Default target - just build the object file
all: $(BUILD_DIR) $(REACT_COMMON_OBJ)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile react_common object file
$(REACT_COMMON_OBJ): $(REACT_COMMON_SRC) $(REACT_COMMON_HDR)
	$(CC) $(CFLAGS) -I../ir -I$(CJSON_DIR) -c $(REACT_COMMON_SRC) -o $(REACT_COMMON_OBJ)

# Also provide a local object file for tsx/jsx to depend on
react_common.o: $(REACT_COMMON_SRC) $(REACT_COMMON_HDR)
	$(CC) $(CFLAGS) -I../ir -I$(CJSON_DIR) -c $(REACT_COMMON_SRC) -o react_common.o
	cp react_common.o $(BUILD_DIR)/

# Clean
clean:
	rm -f react_common.o $(REACT_COMMON_OBJ)

# Install header
install: all
	cp $(REACT_COMMON_HDR) $(BUILD_DIR)/

.PHONY: all clean install
