# SDL3 Renderer Makefile
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
RENDERER_NAME = libkryon_sdl3
STATIC_LIB = $(BUILD_DIR)/$(RENDERER_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(RENDERER_NAME).so

# ============================================================================
# Compiler Configuration
# ============================================================================

CFLAGS_BASE = -std=c99 -Wall -Wextra -O2 -fPIC
CFLAGS_PLATFORM = -DKRYON_TARGET_PLATFORM=0
CFLAGS_SDL3 = $(shell pkg-config --cflags sdl3 sdl3-ttf 2>/dev/null || echo "-I/usr/include/SDL3 -I/usr/include/SDL3_ttf")
INCLUDES = -I$(CORE_DIR)/core/include

FONTCONFIG_CFLAGS := $(shell pkg-config --cflags fontconfig 2>/dev/null)
FONTCONFIG_LIBS := $(shell pkg-config --libs fontconfig 2>/dev/null)

CFLAGS = $(CFLAGS_BASE) $(CFLAGS_PLATFORM) $(CFLAGS_SDL3) $(INCLUDES)
LDFLAGS = -shared
LIBS = $(shell pkg-config --libs sdl3 sdl3-ttf 2>/dev/null || echo "-lSDL3 -lSDL3_ttf")

ifneq ($(FONTCONFIG_CFLAGS),)
    CFLAGS += $(FONTCONFIG_CFLAGS) -DKRYON_HAS_FONTCONFIG=1
    LIBS += $(FONTCONFIG_LIBS)
else
    CFLAGS += -DKRYON_HAS_FONTCONFIG=0
endif

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
    sdl3_backend.c \
    sdl3_input.c \
    sdl3_fonts.c \
    sdl3_sprite_batch.c \
    sdl3_sprite_backend.c \
    sdl3_effects.c

# Special handling for object files with different paths
OBJECTS = \
    $(BUILD_DIR)/sdl3_sdl3_backend.o \
    $(BUILD_DIR)/sdl3_sdl3_input.o \
    $(BUILD_DIR)/sdl3_sdl3_fonts.o \
    $(BUILD_DIR)/sdl3_sdl3_sprite_batch.o \
    $(BUILD_DIR)/sdl3_sdl3_sprite_backend.o \
    $(BUILD_DIR)/sdl3_sdl3_effects.o

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
	@echo "Creating SDL3 renderer static library: $@"
	$(AR) rcs $@ $^

# Shared library build
$(SHARED_LIB): $(OBJECTS) | $(BUILD_DIR)
	@echo "Creating SDL3 renderer shared library: $@"
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Object files
$(BUILD_DIR)/sdl3_%.o: %.c | $(BUILD_DIR)
	@echo "Compiling SDL3 renderer: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# ============================================================================
# Testing
# ============================================================================

test: static
	@echo "Testing SDL3 renderer..."
	@echo "Note: This requires SDL3 to be installed and a display to be available"
	$(MAKE) -C ../examples test_sdl3

# ============================================================================
# Installation
# ============================================================================

install: static shared
	@echo "Installing Kryon SDL3 renderer"
	install -d $(DESTDIR)/usr/local/lib
	install -m 644 $(STATIC_LIB) $(DESTDIR)/usr/local/lib/
	install -m 755 $(SHARED_LIB) $(DESTDIR)/usr/local/lib/

# ============================================================================
# Utilities
# ============================================================================

# Strip symbols
strip: static shared
	@echo "Stripping symbols from SDL3 renderer"
	$(STRIP) $(STATIC_LIB)
	$(STRIP) $(SHARED_LIB)

# ============================================================================
# Dependencies
# ============================================================================

# Check SDL3 availability
check-sdl3:
	@echo "Checking SDL3 availability..."
	@pkg-config --exists sdl3 || (echo "SDL3 not found via pkg-config" && exit 1)
	@pkg-config --exists sdl3-ttf || (echo "SDL3_ttf not found via pkg-config" && exit 1)
	@echo "SDL3 and SDL3_ttf are available"
	@echo "SDL3 version: $$(pkg-config --modversion sdl3)"
	@echo "SDL3_ttf version: $$(pkg-config --modversion sdl3-ttf)"

# ============================================================================
# Cleaning
# ============================================================================

clean:
	@echo "Cleaning SDL3 renderer build artifacts"
	rm -f $(BUILD_DIR)/sdl3_*.o
	rm -f $(STATIC_LIB)
	rm -f $(SHARED_LIB)

# ============================================================================
# Help
# ============================================================================

help:
	@echo "Kryon SDL3 Renderer Build System"
	@echo ""
	@echo "Usage: make [target] [VAR=value]"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build static library (default)"
	@echo "  static     - Build static library"
	@echo "  shared     - Build shared library"
	@echo "  test       - Run tests"
	@echo "  install    - Install libraries"
	@echo "  strip      - Strip symbols from binaries"
	@echo "  check-sdl3 - Check SDL3 availability"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  DEBUG      - Enable debug build (0/1) [default: 0]"
	@echo "  CC         - C compiler"
	@echo "  AR         - Archiver"
	@echo "  STRIP      - Symbol stripper"
	@echo ""
	@echo "Examples:"
	@echo "  make                          # Build static library"
	@echo "  make DEBUG=1                  # Debug build"
	@echo "  make shared                   # Build shared library"
	@echo "  make check-sdl3              # Verify SDL3 is available"

# ============================================================================
# Debug Information
# ============================================================================

info:
	@echo "SDL3 Renderer Build Configuration:"
	@echo "  Compiler: $(CC)"
	@echo "  Flags: $(CFLAGS)"
	@echo "  Sources: $(RENDERER_SOURCES)"
	@echo "  Objects: $(OBJECTS)"
	@echo "  Static Lib: $(STATIC_LIB)"
	@echo "  Shared Lib: $(SHARED_LIB)"
	@echo "  SDL3 CFLAGS: $(CFLAGS_SDL3)"
	@echo "  SDL3 LIBS: $(LIBS)"
