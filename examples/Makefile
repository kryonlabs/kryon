# Kryon Examples Makefile
# Supports Lua examples

# Compiler settings
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -fPIC
LDFLAGS = -L../build -lkryon_core -llua5.4 -lm
INCLUDES = -I../core/include

# Directories
BUILD_DIR = ../build
BIN_DIR = ../bin
LUA_DIR = lua

# Lua C bindings
LUA_BINDINGS = ../bindings/lua/kryon_lua.c

# Find all examples
LUA_EXAMPLES = $(wildcard $(LUA_DIR)/*.lua)

# Convert to targets
LUA_TARGETS = $(patsubst $(LUA_DIR)/%.lua,$(BIN_DIR)/%,$(LUA_EXAMPLES))

# Default target
.PHONY: all
all: lua

# Build all Lua examples
.PHONY: lua
lua: $(LUA_TARGETS)

# Build Lua examples (standalone executables)
$(BIN_DIR)/%: $(LUA_DIR)/%.lua $(LUA_BINDINGS)
	@echo "Building Lua example: $<"
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(LUA_BINDINGS) $(LDFLAGS) -DLUA_SCRIPT=\"$<\"

# Build Lua C bindings as shared library
.PHONY: lua-bindings
lua-bindings: $(BUILD_DIR)/libkryon_lua.so

$(BUILD_DIR)/libkryon_lua.so: $(LUA_BINDINGS)
	@echo "Building Lua C bindings shared library"
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning build artifacts"
	rm -rf $(BIN_DIR)/*
	rm -rf $(BUILD_DIR)/libkryon_lua.so

# Clean specific targets
.PHONY: clean-lua
clean-lua:
	@echo "Cleaning Lua examples"
	rm -f $(LUA_TARGETS)
	rm -f $(BUILD_DIR)/libkryon_lua.so

# Help target
.PHONY: help
help:
	@echo "Kryon Examples Makefile"
	@echo "======================"
	@echo ""
	@echo "Available targets:"
	@echo "  all          - Build all Lua examples"
	@echo "  lua          - Build Lua examples only"
	@echo "  lua-bindings - Build Lua C bindings as shared library"
	@echo "  clean        - Clean all build artifacts"
	@echo "  clean-lua    - Clean Lua examples only"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - GCC compiler"
	@echo "  - Lua development libraries (liblua5.4-dev on Ubuntu)"
	@echo "  - Kryon C core (make -C ../core)"
	@echo ""
	@echo "Examples:"
	@echo "  Build all:          make"
	@echo "  Build Lua only:     make lua"
	@echo "  Build specific:     make $(BIN_DIR)/hello_world"
	@echo "  Run example:        $(BIN_DIR)/hello_world"

# List available examples
.PHONY: list
list:
	@echo "Lua Examples:"
	@for f in $(LUA_EXAMPLES); do echo "  $$f"; done

# Dependencies
$(LUA_TARGETS): | $(BUILD_DIR)/libkryon_core.a

# Build Kryon C core if not exists
$(BUILD_DIR)/libkryon_core.a:
	@echo "Building Kryon C core..."
	@$(MAKE) -C ../core

# Install dependencies info
.PHONY: deps-info
deps-info:
	@echo "Dependencies Information:"
	@echo "========================"
	@echo ""
	@echo "System dependencies:"
	@echo "  Ubuntu/Debian:"
	@echo "    sudo apt install liblua5.4-dev lua5.4"
	@echo ""
	@echo "  macOS:"
	@echo "    brew install lua"
	@echo ""
	@echo "  Windows:"
	@echo "    - Install Lua from https://lua.org/"
	@echo ""
	@echo "Kryon dependencies:"
	@echo "  - C core must be built first: make -C ../core"
	@echo "  - Kryon modules should be in ../src/"
	@echo ""
	@echo "Optional backends:"
	@echo "  - SDL2: libsdl2-dev libsdl2-ttf-dev (Ubuntu)"
