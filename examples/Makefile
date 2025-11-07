# Kryon Examples Makefile
# Supports both Nim and Lua examples

# Compiler settings
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -fPIC
LDFLAGS = -L../build -lkryon_core -llua5.4 -lm
INCLUDES = -I../core/include

# Directories
BUILD_DIR = ../build
BIN_DIR = ../bin
NIM_DIR = nim
LUA_DIR = lua

# Nim compiler settings
NIM = nim
NIM_FLAGS = -d:release --outdir:$(BIN_DIR)

# Lua C bindings
LUA_BINDINGS = ../bindings/lua/kryon_lua.c

# Find all examples
NIM_EXAMPLES = $(wildcard $(NIM_DIR)/*.nim)
LUA_EXAMPLES = $(wildcard $(LUA_DIR)/*.lua)

# Convert to targets
NIM_TARGETS = $(patsubst $(NIM_DIR)/%.nim,$(BIN_DIR)/%,$(NIM_EXAMPLES))
LUA_TARGETS = $(patsubst $(LUA_DIR)/%.lua,$(BIN_DIR)/%,$(LUA_EXAMPLES))

# Default target
.PHONY: all
all: nim lua

# Build all Nim examples
.PHONY: nim
nim: $(NIM_TARGETS)

# Build all Lua examples
.PHONY: lua
lua: $(LUA_TARGETS)

# Build Nim examples
$(BIN_DIR)/%: $(NIM_DIR)/%.nim
	@echo "Building Nim example: $<"
	@mkdir -p $(BIN_DIR)
	$(NIM) c $(NIM_FLAGS) $<

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
.PHONY: clean-nim
clean-nim:
	@echo "Cleaning Nim examples"
	rm -f $(NIM_TARGETS)

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
	@echo "  all          - Build all Nim and Lua examples"
	@echo "  nim          - Build Nim examples only"
	@echo "  lua          - Build Lua examples only"
	@echo "  lua-bindings - Build Lua C bindings as shared library"
	@echo "  clean        - Clean all build artifacts"
	@echo "  clean-nim    - Clean Nim examples only"
	@echo "  clean-lua    - Clean Lua examples only"
	@echo "  help         - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - Nim compiler (for Nim examples)"
	@echo "  - GCC compiler (for Lua examples)"
	@echo "  - Lua development libraries (liblua5.4-dev on Ubuntu)"
	@echo "  - Kryon C core (make -C ../core)"
	@echo ""
	@echo "Examples:"
	@echo "  Build all:          make"
	@echo "  Build Nim only:     make nim"
	@echo "  Build Lua only:     make lua"
	@echo "  Build specific:     make $(BIN_DIR)/hello_world"
	@echo "  Run example:        $(BIN_DIR)/hello_world"

# List available examples
.PHONY: list
list:
	@echo "Nim Examples:"
	@for f in $(NIM_EXAMPLES); do echo "  $$f"; done
	@echo ""
	@echo "Lua Examples:"
	@for f in $(LUA_EXAMPLES); do echo "  $$f"; done

# Dependencies
$(NIM_TARGETS): | $(BUILD_DIR)/libkryon_core.a
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
	@echo "    sudo apt install nim liblua5.4-dev lua5.4"
	@echo ""
	@echo "  macOS:"
	@echo "    brew install nim lua"
	@echo ""
	@echo "  Windows:"
	@echo "    - Install Nim from https://nim-lang.org/"
	@echo "    - Install Lua from https://lua.org/"
	@echo ""
	@echo "Kryon dependencies:"
	@echo "  - C core must be built first: make -C ../core"
	@echo "  - Kryon modules should be in ../src/"
	@echo ""
	@echo "Optional backends:"
	@echo "  - SDL2: libsdl2-dev libsdl2-ttf-dev (Ubuntu)"
	@echo "  - Raylib: naylib Nim package"