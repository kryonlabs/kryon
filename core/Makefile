# Kryon C Core Makefile
# Supports multiple platforms with strict memory constraints

# ============================================================================
# Configuration
# ============================================================================

# Default target
TARGET ?= desktop

# Compiler
CC ?= gcc
AR ?= ar
STRIP ?= strip

# Source directories
CORE_DIR = .
BUILD_DIR = ../build

# Output
LIB_NAME = libkryon_core
STATIC_LIB = $(BUILD_DIR)/$(LIB_NAME).a
SHARED_LIB = $(BUILD_DIR)/$(LIB_NAME).so

# ============================================================================
# Platform Configuration
# ============================================================================

ifeq ($(TARGET),desktop)
    # Desktop configuration
    CFLAGS_BASE = -std=c99 -Wall -Wextra -O2 -fPIC
    CFLAGS_PLATFORM = -DKRYON_TARGET_PLATFORM=0 -DKRYON_NO_FLOAT=0
    LDFLAGS_PLATFORM = -shared
    INCLUDES_PLATFORM = $(shell pkg-config --cflags sdl3 SDL3_ttf 2>/dev/null || echo "-I/usr/include/SDL3")
    LIBS_PLATFORM = -lm $(shell pkg-config --libs sdl3 SDL3_ttf 2>/dev/null || echo "-lSDL3 -lSDL3_ttf")
endif

ifeq ($(TARGET),mcu)
    # MCU configuration (STM32F4)
    CROSS_PREFIX ?= arm-none-eabi-
    CC = $(CROSS_PREFIX)gcc
    AR = $(CROSS_PREFIX)ar
    STRIP = $(CROSS_PREFIX)strip

    CFLAGS_BASE = -std=c99 -Wall -Wextra -Os -ffunction-sections -fdata-sections
    CFLAGS_PLATFORM = -DKRYON_TARGET_PLATFORM=1 -DKRYON_PLATFORM_MCU -DKRYON_NO_HEAP=1 -DKRYON_NO_FLOAT=1
    CFLAGS_ARCH = -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
    LDFLAGS_PLATFORM = -Wl,--gc-sections
    INCLUDES_PLATFORM =
    LIBS_PLATFORM = -lm -lc -lnosys
endif

ifeq ($(TARGET),web)
    # Web configuration (WASM)
    CC = emcc
    CFLAGS_BASE = -std=c99 -Wall -Wextra -O2
    CFLAGS_PLATFORM = -DKRYON_TARGET_PLATFORM=2 -DKRYON_PLATFORM_WEB
    LDFLAGS_PLATFORM = -shared
    INCLUDES_PLATFORM =
    LIBS_PLATFORM =
endif

# ============================================================================
# Compiler Flags
# ============================================================================

CFLAGS = $(CFLAGS_BASE) $(CFLAGS_PLATFORM) $(CFLAGS_ARCH)
CFLAGS += -Iinclude

# Debug configuration
ifdef DEBUG
    CFLAGS += -g -DDEBUG -O0
else
    CFLAGS += -DNDEBUG
endif

# ============================================================================
# Source Files
# ============================================================================

CORE_SOURCES = \
    component.c \
    event.c \
    layout.c \
    renderer.c \
    util.c \
    style.c \
    components.c \
    canvas.c

OBJECTS = $(CORE_SOURCES:%.c=$(BUILD_DIR)/%.o)

# ============================================================================
# Build Targets
# ============================================================================

.PHONY: all clean static shared install test help

# Default target
all: static

# Static library
static: $(STATIC_LIB)

# Shared library (desktop only)
ifeq ($(TARGET),desktop)
shared: $(SHARED_LIB)
else
shared:
	@echo "Shared library not supported for target: $(TARGET)"
endif

# Static library build
$(STATIC_LIB): $(OBJECTS) | $(BUILD_DIR)
	@echo "Creating static library: $@"
	$(AR) rcs $@ $^

# Shared library build (desktop only)
ifeq ($(TARGET),desktop)
$(SHARED_LIB): $(OBJECTS) | $(BUILD_DIR)
	@echo "Creating shared library: $@"
	$(CC) $(LDFLAGS_PLATFORM) -o $@ $^ $(LIBS_PLATFORM)
endif

# Object files
$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ============================================================================
# Testing
# ============================================================================

test: static
	@echo "Running tests for target: $(TARGET)"
	$(MAKE) -C ../test TARGET=$(TARGET)

# ============================================================================
# Installation
# ============================================================================

install: static
	@echo "Installing Kryon Core for target: $(TARGET)"
	install -d $(DESTDIR)/usr/local/lib
	install -d $(DESTDIR)/usr/local/include/kryon
	install -m 644 $(STATIC_LIB) $(DESTDIR)/usr/local/lib/
	install -m 644 include/kryon.h $(DESTDIR)/usr/local/include/kryon/
ifeq ($(TARGET),desktop)
	install -m 755 $(SHARED_LIB) $(DESTDIR)/usr/local/lib/
endif

# ============================================================================
# Utilities
# ============================================================================

# Size analysis (MCU target)
size: static
ifeq ($(TARGET),mcu)
	@echo "Size analysis for target: $(TARGET)"
	$(CROSS_PREFIX)size $(STATIC_LIB)
	$(CROSS_PREFIX)objdump -h $(STATIC_LIB)
else
	@echo "Size analysis only available for MCU target"
endif

# Strip symbols
strip: static shared
	@echo "Stripping symbols"
	$(STRIP) $(STATIC_LIB)
ifeq ($(TARGET),desktop)
	$(STRIP) $(SHARED_LIB)
endif

# ============================================================================
# Cleaning
# ============================================================================

clean:
	@echo "Cleaning build artifacts"
	rm -rf $(BUILD_DIR)
	find . -name "*.o" -delete
	find . -name "*.a" -delete
	find . -name "*.so" -delete

# ============================================================================
# Help
# ============================================================================

help:
	@echo "Kryon C Core Build System"
	@echo ""
	@echo "Usage: make [target] [VAR=value]"
	@echo ""
	@echo "Targets:"
	@echo "  all        - Build static library (default)"
	@echo "  static     - Build static library"
	@echo "  shared     - Build shared library (desktop only)"
	@echo "  test       - Run tests"
	@echo "  install    - Install library and headers"
	@echo "  size       - Show size analysis (MCU only)"
	@echo "  strip      - Strip symbols from binaries"
	@echo "  clean      - Remove build artifacts"
	@echo "  help       - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  TARGET     - Target platform (desktop, mcu, web) [default: desktop]"
	@echo "  DEBUG      - Enable debug build (0/1) [default: 0]"
	@echo "  CC         - C compiler"
	@echo "  AR         - Archiver"
	@echo "  STRIP      - Symbol stripper"
	@echo ""
	@echo "Examples:"
	@echo "  make                          # Build for desktop"
	@echo "  make TARGET=mcu               # Build for MCU"
	@echo "  make DEBUG=1                  # Debug build"
	@echo "  make TARGET=mcu size          # Build for MCU and show size"

# ============================================================================
# Debug Information
# ============================================================================

info:
	@echo "Build Configuration:"
	@echo "  Target: $(TARGET)"
	@echo "  Compiler: $(CC)"
	@echo "  Flags: $(CFLAGS)"
	@echo "  Sources: $(CORE_SOURCES)"
	@echo "  Objects: $(OBJECTS)"
	@echo "  Static Lib: $(STATIC_LIB)"
ifeq ($(TARGET),desktop)
	@echo "  Shared Lib: $(SHARED_LIB)"
endif
