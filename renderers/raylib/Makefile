# Raylib Renderer Makefile
# Part of Kryon UI Framework

# ============================================================================
# Configuration
# ============================================================================

# Compiler
CC ?= gcc
AR ?= ar
STRIP ?= strip

# Paths
CORE_DIR = ../..
BUILD_DIR = ../../build
RENDERER_DIR = .

# Output
RENDERER_NAME = libkryon_raylib
STATIC_LIB = $(BUILD_DIR)/$(RENDERER_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(RENDERER_NAME).so

# ============================================================================
# Compiler Configuration
# ============================================================================

CFLAGS_BASE = -std=c99 -Wall -Wextra -O2 -fPIC
CFLAGS_PLATFORM = -DKRYON_TARGET_PLATFORM=0
CFLAGS_RAYLIB = $(shell pkg-config --cflags raylib 2>/dev/null || echo "-I/usr/include/raylib -I/usr/local/include")
INCLUDES = -I$(CORE_DIR)/core/include

CFLAGS = $(CFLAGS_BASE) $(CFLAGS_PLATFORM) $(CFLAGS_RAYLIB) $(INCLUDES)
LDFLAGS = -shared
LIBS = $(shell pkg-config --libs raylib 2>/dev/null || echo "-lraylib -lm -lpthread -ldl -lrt -lX11")

# Debug configuration
ifdef DEBUG
    CFLAGS += -g -DDEBUG -O0
else
    CFLAGS += -DNDEBUG
endif

# ============================================================================
# Source Files
# ============================================================================

RENDERER_SOURCES = \
    raylib_backend.c

# Object files
OBJECTS = \
    $(BUILD_DIR)/raylib_raylib_backend.o

# ============================================================================
# Build Targets
# ============================================================================

.PHONY: all clean static shared install test help

# Default target
all: static

# Static library
static: $(STATIC_LIB)

# Shared library
shared: $(SHARED_LIB)

# Static library build
$(STATIC_LIB): $(OBJECTS) | $(BUILD_DIR)
	@echo "Creating Raylib renderer static library: $@"
	$(AR) rcs $@ $^

# Shared library build
$(SHARED_LIB): $(OBJECTS) | $(BUILD_DIR)
	@echo "Creating Raylib renderer shared library: $@"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Object files
$(BUILD_DIR)/raylib_%.o: %.c | $(BUILD_DIR)
	@echo "Compiling Raylib renderer: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# Testing
# ============================================================================

test: static
	@echo "Testing Raylib renderer..."
	@echo "Note: This requires raylib to be installed and a display to be available"

# ============================================================================
# Installation
# ============================================================================

install: static shared
	@echo "Installing Kryon Raylib renderer"
	install -d $(DESTDIR)/usr/local/lib
	install -m 644 $(STATIC_LIB) $(DESTDIR)/usr/local/lib/
	install -m 755 $(SHARED_LIB) $(DESTDIR)/usr/local/lib/

# ============================================================================
# Utilities
# ============================================================================

# Strip symbols
strip: static shared
	@echo "Stripping symbols from Raylib renderer"
	$(STRIP) $(STATIC_LIB)
	$(STRIP) $(SHARED_LIB)

# ============================================================================
# Dependencies
# ============================================================================

# Check Raylib availability
check-raylib:
	@echo "Checking Raylib availability..."
	@pkg-config --exists raylib || (echo "Raylib not found via pkg-config" && exit 1)
	@echo "Raylib is available"
	@echo "Raylib version: $$(pkg-config --modversion raylib)"

# ============================================================================
# Cleaning
# ============================================================================

clean:
	@echo "Cleaning Raylib renderer build artifacts"
	rm -f $(BUILD_DIR)/raylib_*.o
	rm -f $(STATIC_LIB)
	rm -f $(SHARED_LIB)

# ============================================================================
# Help
# ============================================================================

help:
	@echo "Kryon Raylib Renderer Build System"
	@echo ""
	@echo "Usage: make [target] [VAR=value]"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build static library (default)"
	@echo "  static       - Build static library"
	@echo "  shared       - Build shared library"
	@echo "  test         - Run tests"
	@echo "  install      - Install libraries"
	@echo "  strip        - Strip symbols from binaries"
	@echo "  check-raylib - Check Raylib availability"
	@echo "  clean        - Remove build artifacts"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  DEBUG        - Enable debug build (0/1) [default: 0]"
	@echo "  CC           - C compiler"
	@echo "  AR           - Archiver"
	@echo "  STRIP        - Symbol stripper"
	@echo ""
	@echo "Examples:"
	@echo "  make                          # Build static library"
	@echo "  make DEBUG=1                  # Debug build"
	@echo "  make shared                   # Build shared library"
	@echo "  make check-raylib            # Verify Raylib is available"

# ============================================================================
# Debug Information
# ============================================================================

info:
	@echo "Raylib Renderer Build Configuration:"
	@echo "  Compiler: $(CC)"
	@echo "  Flags: $(CFLAGS)"
	@echo "  Sources: $(RENDERER_SOURCES)"
	@echo "  Objects: $(OBJECTS)"
	@echo "  Static Lib: $(STATIC_LIB)"
	@echo "  Shared Lib: $(SHARED_LIB)"
	@echo "  Raylib CFLAGS: $(CFLAGS_RAYLIB)"
	@echo "  Raylib LIBS: $(LIBS)"
