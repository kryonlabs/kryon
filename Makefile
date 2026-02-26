# Kryon Core - Makefile
# C89/C90 compliant build using tcc or gcc

# Detect compiler
CC := $(shell command -v tcc 2>/dev/null)
ifeq ($(CC),)
    CC = gcc
endif

CFLAGS = -std=c89 -Wall -Wpedantic -g
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
else
    # In Nix, SDL2 is handled by NIX_LDFLAGS
    SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null || echo "")
    SDL2_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null || echo "-lSDL2")
    SDL2_RPATH :=
endif

ifeq ($(SDL2_CFLAGS),)
    HAVE_SDL2 = 0
    $(warning SDL2 not detected - display client will not build)
else
    HAVE_SDL2 = 1
    CFLAGS += -DHAVE_SDL2 $(SDL2_CFLAGS)
    DISPLAY_LDFLAGS := $(SDL2_LIBS) $(SDL2_RPATH)
endif

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
CORE_SRCS = $(wildcard $(SRC_DIR)/core/*.c)
GRAPHICS_SRCS = $(SRC_DIR)/core/memimage.c $(SRC_DIR)/core/memdraw.c \
                $(SRC_DIR)/core/pixconv.c \
                $(SRC_DIR)/core/devscreen.c $(SRC_DIR)/core/devmouse.c \
                $(SRC_DIR)/core/devdraw.c $(SRC_DIR)/core/devcons.c \
                $(SRC_DIR)/core/render.c $(SRC_DIR)/core/events.c \
                $(SRC_DIR)/core/devkbd.c $(SRC_DIR)/core/devfd.c \
                $(SRC_DIR)/core/devproc.c $(SRC_DIR)/core/devenv.c
# CPU server and namespace support
CPU_SRCS = $(SRC_DIR)/core/cpu_server.c $(SRC_DIR)/core/namespace.c \
            $(SRC_DIR)/core/rcpu.c \
            $(SRC_DIR)/shell/rc_wrapper.c
# Authentication support
AUTH_SRCS = $(SRC_DIR)/core/auth_session.c $(SRC_DIR)/core/factotum_keys.c \
            $(SRC_DIR)/core/devfactotum.c $(SRC_DIR)/core/auth_p9any.c \
            $(SRC_DIR)/core/auth_dp9ik.c $(SRC_DIR)/core/secstore.c
TRANSPORT_SRCS = $(wildcard $(SRC_DIR)/transport/*.c)
CLIENT_SRCS = $(SRC_DIR)/client/9pclient.c $(SRC_DIR)/client/sdl_display.c \
              $(SRC_DIR)/client/eventpoll.c
SRCS = $(CORE_SRCS) $(GRAPHICS_SRCS) $(TRANSPORT_SRCS) $(CPU_SRCS) \
       $(AUTH_SRCS) \
       $(SRC_DIR)/client/9pclient.c

# Additional object files for linking
WINDOW_OBJS = $(BUILD_DIR)/core/window.o
WIDGET_OBJS = $(BUILD_DIR)/core/widget.o

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Client object files
CLIENT_OBJS = $(CLIENT_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Targets
LIB_TARGET = $(BUILD_DIR)/libkryon.a
SERVER_TARGET = $(BIN_DIR)/kryon-server
DISPLAY_TARGET = $(BIN_DIR)/kryon-display

# Default target
.PHONY: all
all: $(LIB_TARGET) $(SERVER_TARGET)
ifneq ($(HAVE_SDL2),0)
all: $(DISPLAY_TARGET)
endif

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/core
	mkdir -p $(BUILD_DIR)/shell
	mkdir -p $(BUILD_DIR)/transport
	mkdir -p $(BUILD_DIR)/client

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Library
$(LIB_TARGET): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

# Server binary
# Use GCC for binaries to get proper RPATH support in Nix
$(SERVER_TARGET): $(SRC_DIR)/server/main.c $(LIB_TARGET) | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g $(CFLAGS) -DINCLUDE_CPU_SERVER -DINCLUDE_NAMESPACE -I$(INCLUDE_DIR) -I$(SRC_DIR)/transport -I$(SRC_DIR) $< -L$(BUILD_DIR) -lkryon -o $@ $(LDFLAGS)

# Display client binary
$(DISPLAY_TARGET): $(SRC_DIR)/client/main.c $(CLIENT_OBJS) $(LIB_TARGET) | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g $(CFLAGS) -I$(INCLUDE_DIR) -I$(SRC_DIR)/transport $< $(CLIENT_OBJS) -L$(BUILD_DIR) -lkryon -o $@ $(DISPLAY_LDFLAGS)

# SDL2 viewer (connects to server via 9P)
# Note: Must use GCC for SDL2 because TCC doesn't support immintrin.h
# We need a separate GCC-compiled library to avoid mixing TCC and GCC object files
SDL_LIB_TARGET = $(BUILD_DIR)/libkryon_gcc.a
SDL_SRCS = $(CORE_SRCS) $(GRAPHICS_SRCS)
SDL_OBJS = $(SDL_SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%_gcc.o)

$(BUILD_DIR)/%_gcc.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	gcc -std=c89 -Wall -Wpedantic -g -I$(INCLUDE_DIR) -c $< -o $@

$(SDL_LIB_TARGET): $(SDL_OBJS)
	ar rcs $@ $^

SDL_VIEWER = $(BIN_DIR)/sdl_viewer
$(SDL_VIEWER): $(SRC_DIR)/client/sdl_viewer.c $(BUILD_DIR)/client/9pclient_gcc.o $(SDL_LIB_TARGET) | $(BIN_DIR)
	gcc -std=c89 -Wall -Wpedantic -g -I$(INCLUDE_DIR) $< $(BUILD_DIR)/client/9pclient_gcc.o -L$(BUILD_DIR) -lkryon_gcc -o $@ $(SDL2_CFLAGS) $(SDL2_LIBS) $(SDL2_RPATH)

.PHONY: run-sdl
run-sdl: $(SDL_VIEWER)
	@echo "Starting SDL2 viewer..."
	@$(SDL_VIEWER)

# Compile object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $< -o $@

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

# Run server
.PHONY: run
run: $(SERVER_TARGET)
	./$(SERVER_TARGET) --port 17019

# Run display client
.PHONY: run-display
run-display: $(DISPLAY_TARGET)
	./$(DISPLAY_TARGET) --host 127.0.0.1 --port 17019

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
            $(TEST_DIR)/test_9p_server.c
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

# Run drawing tests
.PHONY: test-draw
test-draw: $(SERVER_TARGET) tests
	@echo "Starting server on port 17019..."
	@./$(SERVER_TARGET) --port 17019 & \
	SERVER_PID=$$!; \
	sleep 1; \
	echo "Running drawing tests..."; \
	echo "Test 1: Image allocation"; \
	$(BIN_DIR)/test_draw_alloc || echo "Test 1 failed"; \
	echo "Test 2: Line drawing"; \
	$(BIN_DIR)/test_draw_line || echo "Test 2 failed"; \
	echo "Test 3: Polygon drawing"; \
	$(BIN_DIR)/test_draw_poly || echo "Test 3 failed"; \
	echo "Test 4: Text rendering"; \
	$(BIN_DIR)/test_draw_text || echo "Test 4 failed"; \
	echo "Test 5: Bit blit"; \
	$(BIN_DIR)/test_draw_blt || echo "Test 5 failed"; \
	sleep 1; \
	kill $$SERVER_PID 2>/dev/null || true; \
	echo "Drawing tests complete"

# Integration test
.PHONY: test-integration
test-integration: $(SERVER_TARGET) $(DISPLAY_TARGET)
	@echo "Starting server on port 17019..."
	@./$(SERVER_TARGET) --port 17019 &
	@SERVER_PID=$$!; \
	sleep 1; \
	echo "Starting display client..."; \
	timeout 5 ./$(DISPLAY_TARGET) --host 127.0.0.1 --port 17019 || true; \
	kill $$SERVER_PID 2>/dev/null || true; \
	echo "Integration test complete"

# Dependencies
.PHONY: deps
deps:
	@echo "Installing Nix dependencies..."
	nix-shell

help:
	@echo "Kryon Core Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build library, server, and display client (default)"
	@echo "  clean          - Remove build artifacts"
	@echo "  run            - Build and run the server"
	@echo "  run-display    - Build and run the display client"
	@echo "  tests          - Build test programs"
	@echo "  test-draw      - Run drawing tests (requires server)"
	@echo "  test           - Run basic 9P mount test"
	@echo "  test-integration - Run full integration test"
	@echo "  deps           - Show Nix shell command"
	@echo ""
	@echo "Compiler: tcc (C89/C90)"
	@echo "Source:   $(SRC_DIR)"
	@echo "Include:  $(INCLUDE_DIR)"
ifeq ($(HAVE_SDL2),1)
	@echo "SDL2:     Detected"
else
	@echo "SDL2:     Not detected (display client will not build)"
endif
