# Kryon Core - Makefile
# C89/C90 compliant build using tcc or gcc

# Detect compiler
CC := $(shell command -v tcc 2>/dev/null)
ifeq ($(CC),)
    CC = gcc
endif

CFLAGS = -std=c89 -Wall -Wpedantic -g
LDFLAGS =

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = bin

# Source files
CORE_SRCS = $(wildcard $(SRC_DIR)/core/*.c)
GRAPHICS_SRCS = $(SRC_DIR)/core/memimage.c $(SRC_DIR)/core/memdraw.c \
                $(SRC_DIR)/core/devscreen.c $(SRC_DIR)/core/devmouse.c \
                $(SRC_DIR)/core/devdraw.c $(SRC_DIR)/core/devcons.c \
                $(SRC_DIR)/core/render.c
TRANSPORT_SRCS = $(wildcard $(SRC_DIR)/transport/*.c)
SRCS = $(CORE_SRCS) $(GRAPHICS_SRCS) $(TRANSPORT_SRCS)

# Additional object files for linking
WINDOW_OBJS = $(BUILD_DIR)/core/window.o
WIDGET_OBJS = $(BUILD_DIR)/core/widget.o

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Targets
LIB_TARGET = $(BUILD_DIR)/libkryon.a
SERVER_TARGET = $(BIN_DIR)/kryon-server

# Default target
.PHONY: all
all: $(LIB_TARGET) $(SERVER_TARGET)

# Create directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/core
	mkdir -p $(BUILD_DIR)/transport

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Library
$(LIB_TARGET): $(OBJS) | $(BUILD_DIR)
	ar rcs $@ $^

# Server binary
$(SERVER_TARGET): $(SRC_DIR)/server/main.c $(LIB_TARGET) | $(BIN_DIR)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I$(SRC_DIR)/transport $< -L$(BUILD_DIR) -lkryon -o $@ $(LDFLAGS)

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

# Test with plan9port tools
.PHONY: test
test: $(SERVER_TARGET)
	@echo "Starting server on port 17019..."
	@./$(SERVER_TARGET) --port 17019 &
	@SERVER_PID=$$!; \
	sleep 1; \
	echo "Testing 9P mount..."; \
	9mount 127.0.0.1!/tmp/kryon /mnt/kryon 2>/dev/null || echo "Mount failed (server not ready?)"; \
	sleep 2; \
	kill $$SERVER_PID 2>/dev/null || true

# Dependencies
.PHONY: deps
deps:
	@echo "Installing Nix dependencies..."
	nix-shell

help:
	@echo "Kryon Core Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all    - Build library and server (default)"
	@echo "  clean  - Remove build artifacts"
	@echo "  run    - Build and run the CPU server"
	@echo "  test   - Run basic 9P mount test"
	@echo "  deps   - Show Nix shell command"
	@echo ""
	@echo "Compiler: tcc (C89/C90)"
	@echo "Source:   $(SRC_DIR)"
	@echo "Include:  $(INCLUDE_DIR)"
