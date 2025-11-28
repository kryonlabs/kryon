# Makefile for Kryon IR System
# Compiles the core IR library that can be used by frontends and backends

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
IR_DIR = .
BUILD_DIR = ../build
INCLUDE_DIR = .

# Source files
IR_SOURCES = ir_builder.c ir_serialization.c ir_style_vars.c ir_layout.c ir_memory.c ir_hashmap.c ir_animation.c
IR_HEADERS = ir_core.h ir_builder.h ir_serialization.h ir_style_vars.h ir_memory.h ir_hashmap.h ir_animation.h

# Output files
IR_STATIC_LIB = $(BUILD_DIR)/libkryon_ir.a
IR_SHARED_LIB = $(BUILD_DIR)/libkryon_ir.so

# Default target
all: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files
ir_builder.o: ir_builder.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_builder.c -o ir_builder.o

ir_serialization.o: ir_serialization.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_serialization.c -o ir_serialization.o

ir_style_vars.o: ir_style_vars.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_style_vars.c -o ir_style_vars.o

ir_layout.o: ir_layout.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_layout.c -o ir_layout.o

ir_memory.o: ir_memory.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_memory.c -o ir_memory.o

ir_hashmap.o: ir_hashmap.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hashmap.c -o ir_hashmap.o

ir_animation.o: ir_animation.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_animation.c -o ir_animation.o -lm

# Create static library
$(IR_STATIC_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o
	$(AR) $(ARFLAGS) $@ ir_builder.o ir_serialization.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o

# Create shared library
$(IR_SHARED_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o
	$(CC) $(CFLAGS) -shared -o $@ ir_builder.o ir_serialization.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o -lm

# Install headers
install-headers: $(BUILD_DIR)
	cp $(IR_HEADERS) $(BUILD_DIR)/

# Clean build artifacts
clean:
	rm -f *.o
	rm -f $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Build test
test: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(BUILD_DIR) -lkryon_ir test_ir.c -o $(BUILD_DIR)/test_ir
	$(BUILD_DIR)/test_ir

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Help
help:
	@echo "Kryon IR System Build Targets:"
	@echo "  all          - Build static and shared libraries"
	@echo "  static       - Build static library only"
	@echo "  shared       - Build shared library only"
	@echo "  test         - Build and run test program"
	@echo "  clean        - Remove build artifacts"
	@echo "  install-headers - Copy headers to build directory"
	@echo "  debug        - Build with debug symbols"
	@echo "  help         - Show this help"

.PHONY: all clean test debug install-headers help