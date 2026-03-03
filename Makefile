# Kryon Core - Makefile (Restructured)
# C89/C90 compliant build using tcc or gcc
# New structure: wm/, tk/, display/, cli/, lib/

# Detect compiler
CC := $(shell command -v tcc 2>/dev/null)
ifeq ($(CC),)
    CC = gcc
endif

CFLAGS = -std=c89 -Wall -Wpedantic -g -D_POSIX_C_SOURCE=200112L
LDFLAGS =

# Detect Nix environment - use NIX_LDFLAGS for proper RPATH
ifneq ($(NIX_LDFLAGS),)
    # Building in nix-shell - use Nix's automatic RPATH
    # Convert -rpath to -Wl,-rpath, for GCC
    # Also use GCC for building library to avoid TCC/GCC mixing issues
    CC = gcc
    # Convert -rpath to -Wl,-rpath, using perl
    LDFLAGS += $(shell echo '$(NIX_LDFLAGS)' | perl -pe 's/-rpath (\S+)/-Wl,-rpath,$1/g')
    CFLAGS += $(NIX_CFLAGS_COMPILE)
    $(info Building in Nix environment - using GCC for proper RPATH)
    $(info NIX_LDFLAGS=$(NIX_LDFLAGS))
    $(info Converted LDFLAGS=$(LDFLAGS))
endif

# SDL2 detection (required for display client)
ifeq ($(NIX_LDFLAGS),)
    # Only detect SDL2 if not in Nix (Nix handles it automatically)
    SDL2_PKG := $(shell pkg-config --cflags sdl2 2>/dev/null)
    ifeq ($(SDL2_PKG),)
        SDL2_PKG := $(shell sdl2-config --cflags 2>/dev/null)
    endif
    SDL2_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null)
    SDL2_LIBDIR := $(shell pkg-config --libs-only-L sdl2 2>/dev/null | sed 's/^-L//' || echo "")

    SDL2_CFLAGS := $(SDL2_PKG)
    ifneq ($(SDL2_LIBDIR),)
        SDL2_RPATH := -Wl,-rpath=$(SDL2_LIBDIR)
    endif

    ifeq ($(SDL2_CFLAGS),)
        HAVE_SDL2 = 0
        $(warning SDL2 not detected - display client will not build)
    else
        HAVE_SDL2 = 1
        CFLAGS += -DHAVE_SDL2 $(SDL2_CFLAGS)
        DISPLAY_LDFLAGS := $(SDL2_LIBS) $(SDL2_RPATH)
    endif
else
    # In Nix, SDL2 is provided by shell.nix - always enable it
    HAVE_SDL2 = 1
    CFLAGS += -DHAVE_SDL2
    # Find SDL2 include path in Nix store
    SDL2_INCLUDE_PATH := $(shell find /nix/store -name "SDL2" -type d 2>/dev/null | grep -v "doc" | head -1 | sed 's|/SDL2||')
    ifneq ($(SDL2_INCLUDE_PATH),)
        SDL2_CFLAGS := -I$(SDL2_INCLUDE_PATH)
        CFLAGS += $(SDL2_CFLAGS)
    else
        SDL2_CFLAGS :=
    endif
    SDL2_LIBS := -lSDL2
    SDL2_RPATH :=
    DISPLAY_LDFLAGS := $(SDL2_LIBS) $(SDL2_RPATH)
    $(info Using SDL2 from Nix environment)
    $(info SDL2_CFLAGS=$(SDL2_CFLAGS))
endif

# Directories (NEW STRUCTURE)
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Component source directories
WM_SRC_DIR = wm/src
TK_SRC_DIR = tk/src
DISPLAY_SRC_DIR = display/src
CLI_SRC_DIR = cli/src
LIB_SRC_DIR = lib/src

# Marrow graphics library (for linking Kryon with Marrow's graphics)
MARROW_DIR = ../marrow
MARROW_INCLUDE = $(MARROW_DIR)/include
MARROW_GRAPHICS_OBJS = $(BUILD_DIR)/marrow/memdraw.o \
                       $(BUILD_DIR)/marrow/memimage.o \
                       $(BUILD_DIR)/marrow/pixconv.o \
                       $(BUILD_DIR)/marrow/devdraw.o

# Source files by component
# Toolkit (tk/) - Core UI system
TK_SRCS = $(TK_SRC_DIR)/widget.c \
          $(TK_SRC_DIR)/window.c \
          $(TK_SRC_DIR)/render.c \
          $(TK_SRC_DIR)/events.c \
          $(TK_SRC_DIR)/layout.c \
          $(TK_SRC_DIR)/namespace.c \
          $(TK_SRC_DIR)/tree.c \
          $(TK_SRC_DIR)/9p.c \
          $(TK_SRC_DIR)/ops.c \
          $(TK_SRC_DIR)/devenv.c \
          $(TK_SRC_DIR)/vdev.c \
          $(TK_SRC_DIR)/devfd.c \
          $(TK_SRC_DIR)/devproc.c \
          $(TK_SRC_DIR)/devcons.c \
          $(TK_SRC_DIR)/ctl.c \
          $(TK_SRC_DIR)/wm.c \
          $(TK_SRC_DIR)/parser.c \
          $(TK_SRC_DIR)/target.c \
          $(TK_SRC_DIR)/render/primitives.c \
          $(TK_SRC_DIR)/render/widgets.c \
          $(TK_SRC_DIR)/render/events.c \
          $(TK_SRC_DIR)/render/screen.c \
          $(TK_SRC_DIR)/util/color.c \
          $(TK_SRC_DIR)/util/geom.c

# Library (lib/) - 9P client library
LIB_SRCS = $(LIB_SRC_DIR)/client/p9client.c \
           $(LIB_SRC_DIR)/client/marrow.c

# Display client (display/)
DISPLAY_SRCS = $(DISPLAY_SRC_DIR)/sdl_display.c \
               $(DISPLAY_SRC_DIR)/eventpoll.c

# CLI tools (cli/)
CLI_SRCS = $(CLI_SRC_DIR)/main.c \
           $(CLI_SRC_DIR)/commands.c \
           $(CLI_SRC_DIR)/yaml.c

# Window manager (wm/) - Server component
WM_SRCS = $(WM_SRC_DIR)/main.c

# Combined source files for libkryon.a
SRCS = $(TK_SRCS) $(LIB_SRCS)

# Object files
TK_OBJS = $(TK_SRCS:$(TK_SRC_DIR)/%.c=$(BUILD_DIR)/tk/%.o)
LIB_OBJS = $(LIB_SRCS:$(LIB_SRC_DIR)/%.c=$(BUILD_DIR)/lib/%.o)
DISPLAY_OBJS = $(DISPLAY_SRCS:$(DISPLAY_SRC_DIR)/%.c=$(BUILD_DIR)/display/%.o)
CLI_OBJS = $(CLI_SRCS:$(CLI_SRC_DIR)/%.c=$(BUILD_DIR)/cli/%.o)

# All library objects
OBJS = $(TK_OBJS) $(LIB_OBJS)

# Targets
LIB_TARGET = $(BUILD_DIR)/libkryon.a
DISPLAY_TARGET = $(BIN_DIR)/kryon-view
KRYON_WM_TARGET = $(BIN_DIR)/kryon-wm
KRYON_TARGET = $(BIN_DIR)/kryon
SAVE_PPM_TARGET = $(BIN_DIR)/save_ppm

# Default target
.PHONY: all
all: $(LIB_TARGET) $(KRYON_WM_TARGET) $(SAVE_PPM_TARGET)
ifneq ($(HAVE_SDL2),0)
all: $(DISPLAY_TARGET) $(KRYON_TARGET)
endif

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/tk
	mkdir -p $(BUILD_DIR)/tk/util
	mkdir -p $(BUILD_DIR)/tk/render
	mkdir -p $(BUILD_DIR)/lib/client
	mkdir -p $(BUILD_DIR)/display
	mkdir -p $(BUILD_DIR)/cli
	mkdir -p $(BUILD_DIR)/marrow
	mkdir -p $(BUILD_DIR)/wm

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Library
$(LIB_TARGET): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

# Kryon Window Manager binary (connects to Marrow)
$(KRYON_WM_TARGET): $(WM_SRC_DIR)/main.c $(LIB_TARGET) $(MARROW_GRAPHICS_OBJS) | $(BIN_DIR)
	$(CC) -std=c89 -Wall -Wpedantic -g $(CFLAGS) \
		-I$(INCLUDE_DIR) \
		-I$(MARROW_INCLUDE) \
		-I$(TK_SRC_DIR) \
		-I$(LIB_SRC_DIR) \
		$< \
		$(BUILD_DIR)/lib/client/p9client.o \
		$(BUILD_DIR)/lib/client/marrow.o \
		$(BUILD_DIR)/tk/parser.o \
		$(BUILD_DIR)/tk/target.o \
		$(MARROW_GRAPHICS_OBJS) \
		-L$(BUILD_DIR) -lkryon -o $@ $(LDFLAGS) -lm

# Display client binary
$(DISPLAY_TARGET): $(DISPLAY_SRC_DIR)/main.c $(DISPLAY_OBJS) $(LIB_TARGET) | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g $(CFLAGS) \
		-I$(INCLUDE_DIR) \
		-I$(TK_SRC_DIR) \
		-I$(LIB_SRC_DIR) \
		-I$(MARROW_INCLUDE) \
		$< $(DISPLAY_OBJS) -L$(BUILD_DIR) -lkryon -o $@ $(DISPLAY_LDFLAGS)

# Kryon CLI binary
$(KRYON_TARGET): $(CLI_OBJS) $(BUILD_DIR)/tk/target.o | $(BIN_DIR)
	$(CC) -std=c89 -Wall -Wpedantic -g $(CFLAGS) \
		-I$(INCLUDE_DIR) \
		-I$(CLI_SRC_DIR) \
		-I$(TK_SRC_DIR) \
		$(CLI_OBJS) $(BUILD_DIR)/tk/target.o -o $@ $(LDFLAGS)
	chmod +x $(KRYON_TARGET)

# PPM screenshot tool
$(SAVE_PPM_TARGET): tools/save_ppm.c | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g $< -o $@

# SDL2 viewer (connects to server via 9P)
SDL_LIB_TARGET = $(BUILD_DIR)/libkryon_gcc.a
SDL_SRCS = $(TK_SRCS)
SDL_OBJS = $(SDL_SRCS:$(TK_SRC_DIR)/%.c=$(BUILD_DIR)/tk/%_gcc.o)

$(BUILD_DIR)/tk/%_gcc.o: $(TK_SRC_DIR)/%.c | $(BUILD_DIR)
	gcc -std=c89 -Wall -Wpedantic -g -I$(INCLUDE_DIR) -I$(TK_SRC_DIR) -c $< -o $@

$(SDL_LIB_TARGET): $(SDL_OBJS)
	ar rcs $@ $^

SDL_VIEWER = $(BIN_DIR)/sdl_viewer
$(SDL_VIEWER): $(DISPLAY_SRC_DIR)/sdl_viewer.c $(BUILD_DIR)/display/9pclient_gcc.o $(SDL_LIB_TARGET) | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g \
		-I$(INCLUDE_DIR) \
		-I$(TK_SRC_DIR) \
		$< $(BUILD_DIR)/display/9pclient_gcc.o -L$(BUILD_DIR) -lkryon_gcc -o $@ $(SDL2_CFLAGS) $(SDL2_LIBS) $(SDL2_RPATH)

.PHONY: run-sdl
run-sdl: $(SDL_VIEWER)
	@echo "Starting SDL2 viewer..."
	@$(SDL_VIEWER)

# Compile toolkit object files
$(BUILD_DIR)/tk/%.o: $(TK_SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/tk/render
	@mkdir -p $(BUILD_DIR)/tk/util
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(TK_SRC_DIR) -I$(MARROW_INCLUDE) -c $< -o $@

# Compile library object files
$(BUILD_DIR)/lib/client/%.o: $(LIB_SRC_DIR)/client/%.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/lib/client
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(LIB_SRC_DIR) -c $< -o $@

# Compile display client object files
$(BUILD_DIR)/display/%.o: $(DISPLAY_SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/display
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(TK_SRC_DIR) -I$(LIB_SRC_DIR) -c $< -o $@

# Compile CLI object files
$(BUILD_DIR)/cli/%.o: $(CLI_SRC_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/cli
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(CLI_SRC_DIR) -I$(TK_SRC_DIR) -c $< -o $@

# Build Marrow graphics objects (for linking with Kryon)
$(BUILD_DIR)/marrow:
	@mkdir -p $(BUILD_DIR)/marrow

$(BUILD_DIR)/marrow/memdraw.o: $(MARROW_DIR)/lib/graphics/memdraw.c | $(BUILD_DIR)/marrow
	$(CC) $(CFLAGS) -I$(MARROW_INCLUDE) -c $< -o $@

$(BUILD_DIR)/marrow/memimage.o: $(MARROW_DIR)/lib/graphics/memimage.c | $(BUILD_DIR)/marrow
	$(CC) $(CFLAGS) -I$(MARROW_INCLUDE) -c $< -o $@

$(BUILD_DIR)/marrow/pixconv.o: $(MARROW_DIR)/lib/graphics/pixconv.c | $(BUILD_DIR)/marrow
	$(CC) $(CFLAGS) -I$(MARROW_INCLUDE) -c $< -o $@

$(BUILD_DIR)/marrow/devdraw.o: $(MARROW_DIR)/sys/devdraw.c | $(BUILD_DIR)/marrow
	$(CC) $(CFLAGS) -I$(MARROW_INCLUDE) -c $< -o $@

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Convert screenshot to PPM
.PHONY: convert-ppm
convert-ppm: $(SAVE_PPM_TARGET)
	@echo "Converting WM screenshot..."
	@$(SAVE_PPM_TARGET) /tmp/wm_before.raw 800 600 /tmp/wm_screenshot.ppm || echo "No WM screenshot found"
	@echo "Converting Display screenshot..."
	@$(SAVE_PPM_TARGET) /tmp/display_after.raw 800 600 /tmp/display_screenshot.ppm || echo "No Display screenshot found"
	@echo "Done! View with: xdg-open /tmp/wm_screenshot.ppm /tmp/display_screenshot.ppm"

# Run display client
.PHONY: run-display
run-display: $(DISPLAY_TARGET)
	./$(DISPLAY_TARGET) --host 127.0.0.1 --port 17010

# Run Kryon window manager (connects to Marrow)
.PHONY: run run-wm
run: $(KRYON_WM_TARGET)
	./$(KRYON_WM_TARGET)

run-wm: $(KRYON_WM_TARGET)
	./$(KRYON_WM_TARGET)

# Test 9P server
.PHONY: test
test: $(BIN_DIR)/test_9p_server
	@echo "Cleaning up any existing servers..."
	@pkill -9 'kryon-server --port 17020' 2>/dev/null || true
	@pkill -9 test_9p_server 2>/dev/null || true
	@sleep 1
	@echo "Running 9P server tests..."
	@$(BIN_DIR)/test_9p_server

# Build test programs
TEST_DIR = tests
TEST_SRCS = $(TEST_DIR)/test_draw_alloc.c $(TEST_DIR)/test_draw_blt.c \
            $(TEST_DIR)/test_draw_line.c $(TEST_DIR)/test_draw_poly.c \
            $(TEST_DIR)/test_draw_text.c \
            $(TEST_DIR)/test_9p_server.c \
            $(TEST_DIR)/test_namespace.c
TEST_BINS = $(TEST_SRCS:$(TEST_DIR)/%.c=$(BIN_DIR)/%)

# SDL2 test program (for direct visualization)
SDL_TEST = $(BIN_DIR)/sdl_draw_test

.PHONY: tests
tests: $(TEST_BINS)

$(BIN_DIR)/%: $(TEST_DIR)/%.c $(LIB_TARGET) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $< -L$(BUILD_DIR) -lkryon -o $@ $(LDFLAGS) -lm

# SDL2 test (direct visualization)
$(SDL_TEST): $(TEST_DIR)/sdl_draw_test.c $(LIB_TARGET) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $< -L$(BUILD_DIR) -lkryon -o $@ $(LDFLAGS) -lm -lSDL2

.PHONY: test-sdl
test-sdl: $(SDL_TEST)
	@echo "Starting SDL2 visualization test..."
	@$(SDL_TEST)

# Display integration test (builds Marrow first)
.PHONY: test-display
test-display: $(BIN_DIR)/test_display
	@echo "Building Marrow..."
	@cd ../marrow && $(MAKE) clean && $(MAKE)
	@echo "Starting display integration test..."
	@$(BIN_DIR)/test_display

# Add test_display to TEST_BINS if not already included
TEST_DISPLAY_BIN = $(BIN_DIR)/test_display

$(TEST_DISPLAY_BIN): $(TEST_DIR)/test_display.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) $< -o $@ $(LDFLAGS)

# Dependencies
.PHONY: deps
deps:
	@echo "Installing Nix dependencies..."
	nix-shell

help:
	@echo "Kryon Core Build System (Restructured)"
	@echo ""
	@echo "New Structure:"
	@echo "  wm/      - Window manager"
	@echo "  tk/      - UI Toolkit"
	@echo "  display/ - Display client"
	@echo "  cli/     - CLI tools"
	@echo "  lib/     - 9P client library"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build library and display client (default)"
	@echo "  clean          - Remove build artifacts"
	@echo "  run-display    - Build and run the display client"
	@echo "  test-display   - Build Marrow and run integration test"
	@echo "  tests          - Build test programs"
	@echo "  test           - Run basic 9P mount test"
	@echo "  deps           - Show Nix shell command"
	@echo ""
	@echo "Compiler: tcc (C89/C90)"
ifeq ($(HAVE_SDL2),1)
	@echo "SDL2:     Detected"
else
	@echo "SDL2:     Not detected (display client will not build)"
endif
