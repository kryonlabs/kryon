# Makefile for Kryon Desktop Backend
# Compiles IR to native desktop applications using SDL3
# Phase 1, Week 2: Added incremental build support with dependency tracking

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
# Dependency tracking flags (5-10x faster rebuilds)
DEPFLAGS = -MMD -MP
AR = ar
ARFLAGS = rcs

# SDL3 Configuration (adjust paths as needed)
SDL3_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null || echo "-I/usr/include/SDL3")
SDL3_LIBS = $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")
SDL_TTF_LIBS = $(shell pkg-config --libs SDL3_ttf 2>/dev/null || echo "-lSDL3_ttf")

# Raylib Configuration
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib 2>/dev/null || echo "-I/usr/include")
RAYLIB_LIBS = $(shell pkg-config --libs raylib 2>/dev/null || echo "-lraylib -lm")

# Text shaping libraries (HarfBuzz, FreeType, FriBidi)
HARFBUZZ_CFLAGS = $(shell pkg-config --cflags harfbuzz freetype2 fribidi 2>/dev/null || echo "")
HARFBUZZ_LIBS = $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || echo "")

# Enable/disable renderer support (can enable BOTH)
# ENABLE_SDL3 = 1
# ENABLE_RAYLIB = 1

# Default to SDL3 if nothing specified
ifndef ENABLE_SDL3
ifndef ENABLE_RAYLIB
ENABLE_SDL3 = 1
endif
endif

# Directories
DESKTOP_DIR = .
IR_DIR = ../../ir
BUILD_DIR = ../../build
INCLUDE_DIR = .
INCLUDE_IR_DIR = ../../ir
RENDERERS_DIR = ../../renderers
COMMAND_BUF_DIR = $(RENDERERS_DIR)/common

# Conditional compilation flags - allow both backends
ifeq ($(ENABLE_SDL3),1)
    CFLAGS += -DENABLE_SDL3 $(SDL3_CFLAGS)
    LIBS += $(SDL3_LIBS) $(SDL_TTF_LIBS)
endif

ifeq ($(ENABLE_RAYLIB),1)
    CFLAGS += -DENABLE_RAYLIB $(RAYLIB_CFLAGS)
    LIBS += $(RAYLIB_LIBS)
endif

# Add text shaping libraries
CFLAGS += $(HARFBUZZ_CFLAGS)
LIBS += $(HARFBUZZ_LIBS)

# Error if no backend enabled
ifndef ENABLE_SDL3
ifndef ENABLE_RAYLIB
    $(error At least one renderer backend must be enabled: ENABLE_SDL3=1 or ENABLE_RAYLIB=1)
endif
endif

# Source files (modular architecture with abstraction layer)
# Core platform-agnostic sources
DESKTOP_SOURCES = ir_desktop_renderer.c desktop_globals.c desktop_platform.c \
                  desktop_input_helpers.c desktop_test_events.c debug_backend.c \
                  ir_to_commands.c ir_wireframe.c ir_desktop_state.c

# Always add stb_image_impl for stbi_write_png (needed for screenshots)
DESKTOP_SOURCES += stb_image_impl.c

# SDL3 renderer modules (backend + desktop-specific implementation)
SDL3_RENDERER_SOURCES = $(RENDERERS_DIR)/sdl3/sdl3_backend.c \
                        $(RENDERERS_DIR)/sdl3/sdl3_input.c \
                        $(RENDERERS_DIR)/sdl3/sdl3_fonts.c \
                        $(RENDERERS_DIR)/sdl3/sdl3_effects.c \
                        renderers/sdl3/sdl3_renderer.c \
                        renderers/sdl3/sdl3_layout.c \
                        renderers/sdl3/sdl3_input.c \
                        renderers/sdl3/sdl3_fonts.c \
                        renderers/sdl3/sdl3_effects.c

# Raylib renderer modules (backend + desktop-specific implementation)
RAYLIB_RENDERER_SOURCES = $(RENDERERS_DIR)/raylib/raylib_backend.c \
                          $(RENDERERS_DIR)/raylib/raylib_effects.c \
                          renderers/raylib/raylib_renderer.c \
                          renderers/raylib/raylib_layout.c \
                          renderers/raylib/raylib_input.c \
                          renderers/raylib/raylib_fonts.c

# Fix object file paths for the archive command
# Convert renderers/* paths to ../../renderers/* for ar command
# Only apply to files in renderers/ subdirectory, not ../../renderers/
DESKTOP_OBJS_FIX = $(patsubst renderers/%,../../renderers/%,$(DESKTOP_OBJS))

# Add renderer-specific sources
ifeq ($(ENABLE_SDL3),1)
    DESKTOP_SOURCES += $(SDL3_RENDERER_SOURCES)
endif

ifeq ($(ENABLE_RAYLIB),1)
    DESKTOP_SOURCES += $(RAYLIB_RENDERER_SOURCES)
endif

DESKTOP_HEADERS = ir_desktop_renderer.h desktop_internal.h debug_backend.h ir_wireframe.h

# Output files - conditional based on enabled backend
ifeq ($(ENABLE_RAYLIB),1)
    DESKTOP_STATIC_LIB = $(BUILD_DIR)/libkryon_desktop_raylib.a
    DESKTOP_SHARED_LIB = $(BUILD_DIR)/libkryon_desktop_raylib.so
else
    DESKTOP_STATIC_LIB = $(BUILD_DIR)/libkryon_desktop_sdl3.a
    DESKTOP_SHARED_LIB = $(BUILD_DIR)/libkryon_desktop_sdl3.so
endif
DESKTOP_TEST = $(BUILD_DIR)/desktop_test

# Include paths - add all necessary directories for proper modular includes
INCLUDES = -I$(INCLUDE_DIR) \
           -I$(INCLUDE_IR_DIR) \
           -I$(INCLUDE_IR_DIR)/include \
           -I../../include \
           -I../../core/include \
           -I./renderers/sdl3 \
           -I./renderers/raylib \
           -I$(RENDERERS_DIR)/sdl3 \
           -I$(RENDERERS_DIR)/common \
           -I../../bindings/c \
           -I../../third_party/stb

# Link libraries (excluding IR libs which are added separately)
LINK_LIBS = -L$(BUILD_DIR) -lkryon_ir $(LIBS)

# Default target - build both SDL3 and Raylib libraries
all:
	@echo "Building both SDL3 and Raylib desktop libraries..."
	$(MAKE) clean
	@echo "Building SDL3 desktop library..."
	$(MAKE) build-lib ENABLE_SDL3=1 ENABLE_RAYLIB=0
	@echo "Cleaning object files before raylib build..."
	rm -f *.o *.d renderers/sdl3/*.o renderers/sdl3/*.d renderers/raylib/*.o renderers/raylib/*.d
	@echo "Building Raylib desktop library..."
	$(MAKE) build-lib ENABLE_SDL3=0 ENABLE_RAYLIB=1
	@echo "Done! Built libkryon_desktop_sdl3.a and libkryon_desktop_raylib.a"

# Internal target for building a single library
build-lib: $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object files list - auto-generated from sources
DESKTOP_OBJS = $(DESKTOP_SOURCES:.c=.o)

# Separate parent directory objects from local objects
PARENT_RENDERER_OBJS = $(filter ../../renderers/%,$(DESKTOP_OBJS))
LOCAL_RENDERER_OBJS = $(filter-out ../../renderers/%,$(DESKTOP_OBJS))

# Dependency files (auto-generated)
DESKTOP_DEPS = $(DESKTOP_OBJS:.o=.d)

# Compile object files with dependency tracking
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# Include auto-generated dependencies (if they exist)
-include $(DESKTOP_DEPS)

# Create static library
# Use the parent objects directly and find local objects
$(DESKTOP_STATIC_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	@echo "Creating static library: $@"
	@echo "Parent objects: $(PARENT_RENDERER_OBJS)"
	@echo "Local objects: $(LOCAL_RENDERER_OBJS)"
	@$(AR) $(ARFLAGS) $@ $(PARENT_RENDERER_OBJS) $(shell find renderers -name "*.o" 2>/dev/null || true)

# Create shared library
$(DESKTOP_SHARED_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(DESKTOP_OBJS) \
		-L$(BUILD_DIR) -lkryon_ir -lkryon_command_buf $(LIBS)

# Build test program
$(DESKTOP_TEST): $(DESKTOP_STATIC_LIB) desktop_test.c
	$(CC) $(CFLAGS) $(INCLUDES) desktop_test.c $(LINK_LIBS) -l$(DESKTOP_STATIC_LIB:$(BUILD_DIR)/lib%.a=%) -o $@

# Run test
test: $(DESKTOP_TEST)
	$(DESKTOP_TEST)

# Example desktop rendering
example: $(DESKTOP_STATIC_LIB)
	$(CC) $(CFLAGS) $(INCLUDES) example_desktop_render.c $(LINK_LIBS) -l$(DESKTOP_STATIC_LIB:$(BUILD_DIR)/lib%.a=%) -o $(BUILD_DIR)/example_desktop_render
	$(BUILD_DIR)/example_desktop_render

# Clean build artifacts (including dependency files)
clean:
	rm -f *.o *.d
	rm -f renderers/sdl3/*.o renderers/sdl3/*.d
	rm -f renderers/raylib/*.o renderers/raylib/*.d
	rm -f $(BUILD_DIR)/libkryon_desktop_sdl3.a $(BUILD_DIR)/libkryon_desktop_sdl3.so
	rm -f $(BUILD_DIR)/libkryon_desktop_raylib.a $(BUILD_DIR)/libkryon_desktop_raylib.so
	rm -f $(DESKTOP_TEST) $(BUILD_DIR)/example_desktop_render

# Install headers
install-headers: $(BUILD_DIR)
	cp $(DESKTOP_HEADERS) $(BUILD_DIR)/

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB)

# Sanitizer flag definitions (preserve pkg-config CFLAGS)
ASAN_FLAGS = -g -fsanitize=address -fno-omit-frame-pointer -DDEBUG
UBSAN_FLAGS = -g -fsanitize=undefined -fno-omit-frame-pointer -DDEBUG
TSAN_FLAGS = -g -fsanitize=thread -fno-omit-frame-pointer -DDEBUG

# Sanitizer builds for catching bugs during testing
asan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(ASAN_FLAGS)"

ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(UBSAN_FLAGS)"

asan-ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(ASAN_FLAGS) $(UBSAN_FLAGS)"

tsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(TSAN_FLAGS)"

# Build without SDL3 (header-only mode)
no-sdl: ENABLE_SDL3=0
no-sdl: $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB)

# Build with raylib renderer only
raylib:
	$(MAKE) clean
	$(MAKE) build-lib ENABLE_SDL3=0 ENABLE_RAYLIB=1

# Build with SDL3 renderer only
sdl3:
	$(MAKE) clean
	$(MAKE) build-lib ENABLE_SDL3=1 ENABLE_RAYLIB=0

# Build desktop (defaults to SDL3)
desktop:
	$(MAKE) clean
	$(MAKE) build-lib ENABLE_SDL3=1 ENABLE_RAYLIB=0

# Help
help:
	@echo "Kryon Desktop Backend Build Targets:"
	@echo "  all            - Build BOTH SDL3 and Raylib libraries"
	@echo "  static         - Build static library only"
	@echo "  shared         - Build shared library only"
	@echo "  test           - Build and run test program"
	@echo "  example        - Build and run example desktop renderer"
	@echo "  clean          - Remove build artifacts"
	@echo "  install-headers - Copy headers to build directory"
	@echo "  debug          - Build with debug symbols"
	@echo "  asan           - Build with AddressSanitizer"
	@echo "  ubsan          - Build with UndefinedBehaviorSanitizer"
	@echo "  asan-ubsan     - Build with both ASan and UBSan (recommended)"
	@echo "  tsan           - Build with ThreadSanitizer"
	@echo "  desktop        - Build desktop library (defaults to SDL3)"
	@echo "  sdl3           - Build SDL3 library only (libkryon_desktop_sdl3.a)"
	@echo "  raylib         - Build Raylib library only (libkryon_desktop_raylib.a)"
	@echo "  no-sdl         - Build without SDL3 support (header-only)"
	@echo "  help           - Show this help"
	@echo ""
	@echo "SDL3 Status: $(if $(ENABLE_SDL3),ENABLED,DISABLED)"
	@echo "Raylib Status: $(if $(ENABLE_RAYLIB),ENABLED,DISABLED)"

.PHONY: all build-lib clean test example debug asan ubsan asan-ubsan tsan install-headers help no-sdl desktop sdl3 raylib