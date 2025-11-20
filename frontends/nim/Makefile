# Makefile for Kryon Nim Frontend to IR Compiler

# Directories
NIM_DIR = .
IR_DIR = ../../ir
BUILD_DIR = ../../build
NIM_CACHE_DIR = $(BUILD_DIR)/nimcache

# Nim compiler settings
NIM = nim
NIM_FLAGS = --nimcache:$(NIM_CACHE_DIR) --path:$(IR_DIR) --app:lib --out:$(BUILD_DIR)

# Source files
NIM_SOURCES = ir_compiler.nim logic_compiler.nim

# Build targets
.PHONY: all clean test nim-ir-lib nim-logic-lib help

# Default target
all: nim-ir-lib nim-logic-lib

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
$(NIM_CACHE_DIR):
	mkdir -p $(NIM_CACHE_DIR)

# Build IR library first
ir-lib: $(BUILD_DIR)
	$(MAKE) -C $(IR_DIR) all

# Build Nim IR compiler library
nim-ir-lib: $(NIM_CACHE_DIR) ir-lib
	$(NIM) c $(NIM_FLAGS)/libkryon_nim_ir.so $(NIM_DIR)/ir_compiler.nim

# Build Nim logic compiler library
nim-logic-lib: $(NIM_CACHE_DIR) ir-lib
	$(NIM) c $(NIM_FLAGS)/libkryon_nim_logic.so $(NIM_DIR)/logic_compiler.nim

# Test the compilation
test: nim-ir-lib
	$(NIM) c --run --nimcache:$(NIM_CACHE_DIR) --path:$(IR_DIR) $(NIM_DIR)/ir_compiler.nim

# Clean build artifacts
clean:
	rm -rf $(NIM_CACHE_DIR)
	rm -f $(BUILD_DIR)/libkryon_nim_*.so
	rm -f $(BUILD_DIR)/libkryon_*.so
	$(MAKE) -C $(IR_DIR) clean

# Install dependencies
deps:
	$(NIM) c -r nimble install strformat

# Help
help:
	@echo "Kryon Nim Frontend Build Targets:"
	@echo "  all           - Build both IR and logic compiler libraries"
	@echo "  nim-ir-lib    - Build Nim to IR compiler library"
	@echo "  nim-logic-lib - Build Nim logic to WASM compiler library"
	@echo "  test          - Run test compilation"
	@echo "  ir-lib        - Build C IR library (dependency)"
	@echo "  clean         - Remove build artifacts"
	@echo "  deps          - Install Nim dependencies"
	@echo "  help          - Show this help"