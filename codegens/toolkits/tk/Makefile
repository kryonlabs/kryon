# Tk Toolkit Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../../../build
IR_DIR = ../../ir
CODEGEN_DIR = ../..

# Include paths
INCLUDES = -I$(IR_DIR)/include \
           -I$(CODEGEN_DIR) \
           -I$(CODEGEN_DIR)/wir \
           -I$(CODEGEN_DIR)/common \
           -I../../../third_party/cJSON

# Source files
SRCS = tk_codegen.c \
       tk_wir_emitter.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output library
LIB = $(BUILD_DIR)/libkryon_toolkit_tk.a

# Default target
all: $(LIB)

# Build static library
$(LIB): $(OBJS) | $(BUILD_DIR)
	@echo "Building Tk toolkit codegen library..."
	$(AR) $(ARFLAGS) $@ $^
	@echo "✓ Built Tk toolkit codegen: $(LIB)"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJS) $(LIB)

# Install
install: $(LIB)
	@mkdir -p $(BUILD_DIR)/include/kryon/codegen/toolkits/tk
	cp tk_codegen.h $(BUILD_DIR)/include/kryon/codegen/toolkits/tk/

.PHONY: all clean install
