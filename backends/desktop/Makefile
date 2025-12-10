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

# Enable/disable SDL3 support
ENABLE_SDL3 = 1

# Directories
DESKTOP_DIR = .
IR_DIR = ../../ir
PLUGIN_DIR = ../../plugins
BUILD_DIR = ../../build
INCLUDE_DIR = .
INCLUDE_IR_DIR = ../../ir

# Conditional compilation flags
ifeq ($(ENABLE_SDL3),1)
    CFLAGS += -DENABLE_SDL3 $(SDL3_CFLAGS)
    LIBS += $(SDL3_LIBS) $(SDL_TTF_LIBS)
else
    CFLAGS += -DDISABLE_SDL3
endif

# Source files (modular architecture - 10 modules + plugins)
DESKTOP_SOURCES = ir_desktop_renderer.c desktop_globals.c \
                  desktop_fonts.c desktop_effects.c desktop_layout.c \
                  desktop_input.c desktop_rendering.c desktop_test_events.c \
                  debug_backend.c canvas.c \
                  $(PLUGIN_DIR)/canvas/canvas_plugin.c \
                  $(PLUGIN_DIR)/tilemap/tilemap_plugin.c
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
               desktop_input.o desktop_rendering.o desktop_test_events.o \
               debug_backend.o canvas.o canvas_plugin.o tilemap_plugin.o

# Dependency files (auto-generated)
DESKTOP_DEPS = $(DESKTOP_OBJS:.o=.d)

# Compile object files with dependency tracking
%.o: %.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# Compile plugin files with dependency tracking
canvas_plugin.o: $(PLUGIN_DIR)/canvas/canvas_plugin.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

tilemap_plugin.o: $(PLUGIN_DIR)/tilemap/tilemap_plugin.c
	$(CC) $(CFLAGS) $(DEPFLAGS) $(INCLUDES) -c $< -o $@

# Include auto-generated dependencies (if they exist)
-include $(DESKTOP_DEPS)

# Create static library
$(DESKTOP_STATIC_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	$(AR) $(ARFLAGS) $@ $(DESKTOP_OBJS)

# External plugin paths (optional - use weak linking)
MARKDOWN_PLUGIN_DIR = $(HOME)/Projects/kryon-plugin-markdown
MARKDOWN_PLUGIN_LIB = $(MARKDOWN_PLUGIN_DIR)/build/libkryon_markdown.a

# Check if markdown plugin exists
MARKDOWN_PLUGIN_EXISTS = $(shell [ -f "$(MARKDOWN_PLUGIN_LIB)" ] && echo "yes" || echo "no")

# Add markdown plugin to link flags if it exists
ifeq ($(MARKDOWN_PLUGIN_EXISTS),yes)
    PLUGIN_LIBS = $(MARKDOWN_PLUGIN_LIB)
    $(info Found markdown plugin at $(MARKDOWN_PLUGIN_LIB))
else
    PLUGIN_LIBS =
endif

# Create shared library
$(DESKTOP_SHARED_LIB): $(BUILD_DIR) $(DESKTOP_OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(DESKTOP_OBJS) $(PLUGIN_LIBS) -L$(BUILD_DIR) -lkryon_ir $(LIBS)

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

# Build without SDL3 (header-only mode)
no-sdl: ENABLE_SDL3=0
no-sdl: $(DESKTOP_STATIC_LIB) $(DESKTOP_SHARED_LIB)

# Help
help:
	@echo "Kryon Desktop Backend Build Targets:"
	@echo "  all            - Build static and shared libraries"
	@echo "  static         - Build static library only"
	@echo "  shared         - Build shared library only"
	@echo "  test           - Build and run test program"
	@echo "  example        - Build and run example desktop renderer"
	@echo "  clean          - Remove build artifacts"
	@echo "  install-headers - Copy headers to build directory"
	@echo "  debug          - Build with debug symbols"
	@echo "  no-sdl         - Build without SDL3 support (header-only)"
	@echo "  help           - Show this help"
	@echo ""
	@echo "SDL3 Status: $(if $(ENABLE_SDL3),ENABLED,DISABLED)"

.PHONY: all clean test example debug install-headers help no-sdl