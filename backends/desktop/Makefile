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

# Enable/disable renderer support (can enable BOTH)
# ENABLE_SDL3 = 1
# ENABLE_RAYLIB = 1

# Default to BOTH SDL3 and Raylib if nothing specified
ifndef ENABLE_SDL3
ifndef ENABLE_RAYLIB
ENABLE_SDL3 = 1
ENABLE_RAYLIB = 1
endif
endif

# Directories
DESKTOP_DIR = .
IR_DIR = ../../ir
BUILD_DIR = ../../build
INCLUDE_DIR = .
INCLUDE_IR_DIR = ../../ir

# Conditional compilation flags - allow both backends
ifeq ($(ENABLE_SDL3),1)
    CFLAGS += -DENABLE_SDL3 $(SDL3_CFLAGS)
    LIBS += $(SDL3_LIBS) $(SDL_TTF_LIBS)
endif

ifeq ($(ENABLE_RAYLIB),1)
    CFLAGS += -DENABLE_RAYLIB $(RAYLIB_CFLAGS)
    LIBS += $(RAYLIB_LIBS)
endif

# Error if no backend enabled
ifndef ENABLE_SDL3
ifndef ENABLE_RAYLIB
    $(error At least one renderer backend must be enabled: ENABLE_SDL3=1 or ENABLE_RAYLIB=1)
endif
endif

# Source files (modular architecture - 11 modules)
DESKTOP_SOURCES = ir_desktop_renderer.c desktop_globals.c \
                  desktop_fonts.c desktop_effects.c desktop_layout.c \
                  desktop_input.c desktop_test_events.c \
                  debug_backend.c stb_image_impl.c

# Add renderer-specific sources
ifeq ($(ENABLE_SDL3),1)
    DESKTOP_SOURCES += ir_to_commands.c
endif

ifeq ($(ENABLE_RAYLIB),1)
    DESKTOP_SOURCES += desktop_rendering_raylib.c desktop_init_raylib.c
endif

DESKTOP_HEADERS = ir_desktop_renderer.h desktop_internal.h debug_backend.h

# Output files
DESKTOP_STATIC_LIB = $(BUILD_DIR)/libkryon_desktop.a
DESKTOP_SHARED_LIB = $(BUILD_DIR)/libkryon_desktop.so
DESKTOP_TEST = $(BUILD_DIR)/desktop_test

# Include paths
INCLUDES = -I$(INCLUDE_DIR) -I$(INCLUDE_IR_DIR)

# Link libraries (excluding IR libs which are added separately)
LINK_LIBS = -L$(BUILD_DIR) -lkryon_ir $(LIBS)

# Default target
all: $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Object files list
DESKTOP_OBJS = ir_desktop_renderer.o desktop_globals.o \
               desktop_fonts.o desktop_effects.o desktop_layout.o \
               desktop_input.o desktop_test_events.o \
               debug_backend.o stb_image_impl.o

# Add renderer-specific objects
ifeq ($(ENABLE_SDL3),1)
    DESKTOP_OBJS += ir_to_commands.o
endif

ifeq ($(ENABLE_RAYLIB),1)
    DESKTOP_OBJS += desktop_rendering_raylib.o desktop_init_raylib.o
endif

# Dependency files (auto-generated)
DESKTOP_DEPS = $(DESKTOP_OBJS:.o=.d)

# Compile object files with dependency tracking
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# Include auto-generated dependencies (if they exist)
-include $(DESKTOP_DEPS)

# Create static library
$(DESKTOP_STATIC_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	$(AR) $(ARFLAGS) $@ $(DESKTOP_OBJS)

# Create shared library
$(DESKTOP_SHARED_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(DESKTOP_OBJS) -L$(BUILD_DIR) -lkryon_ir -lkryon_sdl3 -lkryon_common $(LIBS)

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
	rm -f $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB) $(DESKTOP_TEST) $(BUILD_DIR)/example_desktop_render

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
	$(MAKE) all ENABLE_SDL3=0 ENABLE_RAYLIB=1

# Build with SDL3 renderer only
sdl3:
	$(MAKE) clean
	$(MAKE) all ENABLE_SDL3=1 ENABLE_RAYLIB=0

# Build with BOTH renderers (default)
both:
	$(MAKE) clean
	$(MAKE) all ENABLE_SDL3=1 ENABLE_RAYLIB=1

# Help
help:
	@echo "Kryon Desktop Backend Build Targets:"
	@echo "  all            - Build static and shared libraries (default: both renderers)"
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
	@echo "  sdl3           - Build with SDL3 renderer only"
	@echo "  raylib         - Build with Raylib renderer only"
	@echo "  both           - Build with BOTH SDL3 and Raylib renderers"
	@echo "  no-sdl         - Build without SDL3 support (header-only)"
	@echo "  help           - Show this help"
	@echo ""
	@echo "SDL3 Status: $(if $(ENABLE_SDL3),ENABLED,DISABLED)"
	@echo "Raylib Status: $(if $(ENABLE_RAYLIB),ENABLED,DISABLED)"

.PHONY: all clean test example debug asan ubsan asan-ubsan tsan install-headers help no-sdl