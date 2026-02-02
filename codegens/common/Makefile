# Common Codegen Infrastructure Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../build
IR_DIR = ../../ir
CODEGEN_DIR = ..
THIRD_PARTY_DIR = ../../third_party

# Include paths
INCLUDES = -I$(IR_DIR)/include \
           -I$(CODEGEN_DIR) \
           -I$(CODEGEN_DIR)/wir \
           -I$(THIRD_PARTY_DIR)/cJSON

# Source files
SRCS = wir_composer.c \
       color_utils.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output library
LIB = $(BUILD_DIR)/libkryon_codegen_common.a

# Default target
all: $(LIB)

# Build static library
$(LIB): $(OBJS)
	@echo "Building common codegen library..."
	$(AR) $(ARFLAGS) $@ $^
	@echo "✓ Built common codegen: $(LIB)"

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(LIB)

# Install
install: $(LIB)
	@mkdir -p $(BUILD_DIR)/include/kryon/codegen/common
	cp wir_composer.h $(BUILD_DIR)/include/kryon/codegen/common/
	cp color_utils.h $(BUILD_DIR)/include/kryon/codegen/common/

.PHONY: all clean install
