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
IR_SOURCES = ir_builder.c ir_serialization.c
IR_HEADERS = ir_core.h ir_builder.h ir_serialization.h

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

# Create static library
$(IR_STATIC_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o
	$(AR) $(ARFLAGS) $@ ir_builder.o ir_serialization.o

# Create shared library
$(IR_SHARED_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o
	$(CC) $(CFLAGS) -shared -o $@ ir_builder.o ir_serialization.o

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