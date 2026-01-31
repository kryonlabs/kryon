# Kryon Build System (Simplified)
# ==================================
#
# This Makefile builds the entire Kryon framework using shared build rules
# Author: Generated from build system improvement plan
# Date: 2026-01-19

# ============================================================================
# Configuration
# ============================================================================

# Build directories
BUILD_DIR ?= build
PREFIX ?= $(HOME)/.local

# Module directories
CLI_DIR = cli
IR_DIR = ir
CODEGENS_DIR = codegens
TESTS_DIR = tests

# Installation directories
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCLUDEDIR = $(PREFIX)/include/kryon
PKGCONFIGDIR = $(PREFIX)/lib/pkgconfig

# Library extensions
STATIC_LIB_EXT = .a
SHARED_LIB_EXT = .so

# Default target
all: cli ir dis codegens desktop dsl_runtime

# Help
help:
	@echo "Kryon Build System"
	@echo ""
	@echo "Targets:"
	@echo "  vendored     - Build vendored third-party libraries (harfbuzz, freetype, etc.)"
	@echo "  all          - Build everything (default)"
	@echo "  cli          - Build CLI tool"
	@echo "  ir           - Build IR library"
	@echo "  codegens     - Build all code generators"
	@echo "  clean        - Remove all build artifacts"
	@echo "  install      - Install to $(PREFIX)"
	@echo ""
	@echo "Installation:"
	@echo "  PREFIX=/path   - Installation prefix (default: ~/.local)"
	@echo "  Example: make PREFIX=/usr/local install"
	@echo "  Example: make install  (installs to ~/.local)"
	@echo ""
	@echo "Development:"
	@echo "  test         - Run test suite"
	@echo "  check        - Run static analysis"
	@echo "  docs         - Generate documentation"

# ============================================================================
# Module Targets
# ============================================================================

# Build vendored third-party libraries
# Most libraries now use system packages via shell.nix
# Only cJSON and tomlc99 are still vendored (too small to bother with packages)
vendored:
	@echo "Vendored libraries now use system packages (see shell.nix)"
	@echo "System libraries: harfbuzz, freetype, fribidi, SDL3, raylib"
	@echo "Vendored libraries: cJSON, tomlc99, stb"
	@echo "All dependencies available via nix-shell"

# CLI tool (builds the main kryon binary)
cli: ir codegens
	@echo "Building CLI tool..."
	@$(MAKE) -C $(CLI_DIR) all
	@echo "✓ Built CLI"

# IR library (core intermediate representation)
ir:
	@echo "Building IR library..."
	@$(MAKE) -C $(IR_DIR) static
	@echo "✓ Built IR library"

# Code generators (C, Kry, Web, Limbo, Android)
codegens: ir
	@echo "Building code generators..."
	@$(MAKE) -C $(CODEGENS_DIR)/c all
	@$(MAKE) -C $(CODEGENS_DIR)/kry all
	@$(MAKE) -C $(CODEGENS_DIR)/markdown all
	@$(MAKE) -C $(CODEGENS_DIR)/web all
	@$(MAKE) -C $(CODEGENS_DIR)/limbo all
	@$(MAKE) -C $(CODEGENS_DIR)/android all
	@echo "✓ Built code generators"

# DIS bytecode compiler (top-level directory, NOT a codegen)
dis: ir
	@echo "Building DIS bytecode compiler..."
	@$(MAKE) -C dis all
	@echo "✓ Built DIS"

# Desktop runtime (SDL3 renderer)
desktop: ir
	@echo "Building desktop runtime..."
	@$(MAKE) -C renderers/common all
	@$(MAKE) -C runtime/desktop all
	@echo "✓ Built desktop runtime"

# DSL runtime for desktop target
dsl_runtime: desktop
	@echo "Building DSL runtime..."
	@$(MAKE) -C bindings/c dsl_runtime
	@$(MAKE) -C bindings/c install
	@echo "✓ Built DSL runtime"

# ============================================================================
# Clean Target
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@rm -rf $(BUILD_DIR)/harfbuzz $(BUILD_DIR)/freetype
	@$(MAKE) -C $(IR_DIR) clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/c clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/kry clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/markdown clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/web clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/limbo clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/android clean || true
	@$(MAKE) -C dis clean || true
	@$(MAKE) -C $(CLI_DIR) clean || true
	@$(MAKE) -C runtime/desktop clean || true
	@$(MAKE) -C renderers/common clean || true
	@$(MAKE) -C bindings/c clean || true
	@echo "✓ Clean complete"

# ============================================================================
# Install Target
# ============================================================================

install: all
	@echo "Installing Kryon to $(PREFIX)..."
	@mkdir -p $(BINDIR) $(LIBDIR) $(INCLUDEDIR) $(PKGCONFIGDIR)

	# Install CLI binary
	@if [ -f $(CLI_DIR)/kryon ]; then \
		cp $(CLI_DIR)/kryon $(BINDIR)/; \
		echo "  Installed $(BINDIR)/kryon"; \
	fi

	# Install ALL libraries to share directory
	@mkdir -p $(HOME)/.local/share/kryon/build
	@cp $(BUILD_DIR)/*.so $(HOME)/.local/share/kryon/build/ 2>/dev/null || true
	@cp $(BUILD_DIR)/*.a $(HOME)/.local/share/kryon/build/ 2>/dev/null || true
	@cp bindings/c/*.a $(HOME)/.local/share/kryon/build/ 2>/dev/null || true

	# Install shared libraries to lib directory (for dynamic linker)
	@cp $(BUILD_DIR)/*.so $(LIBDIR)/ 2>/dev/null || true

	# Install headers
	@mkdir -p $(HOME)/.local/share/kryon/ir/include
	@cp -r ir/include/*.h $(HOME)/.local/share/kryon/ir/include/
	@mkdir -p $(HOME)/.local/share/kryon/core/include
	@cp -r core/include/*.h $(HOME)/.local/share/kryon/core/include/ 2>/dev/null || true
	@mkdir -p $(HOME)/.local/share/kryon/include/kryon
	@cp -r include/kryon/*.h $(HOME)/.local/share/kryon/include/kryon/

	# Install renderers (CRITICAL - for desktop targets)
	@mkdir -p $(HOME)/.local/share/kryon/renderers/sdl3
	@mkdir -p $(HOME)/.local/share/kryon/renderers/raylib
	@mkdir -p $(HOME)/.local/share/kryon/renderers/common
	@cp -r renderers/sdl3/*.h $(HOME)/.local/share/kryon/renderers/sdl3/ 2>/dev/null || true
	@cp -r renderers/raylib/*.h $(HOME)/.local/share/kryon/renderers/raylib/ 2>/dev/null || true
	@cp -r renderers/common/*.h $(HOME)/.local/share/kryon/renderers/common/ 2>/dev/null || true

	# Install runtime headers (CRITICAL - for desktop targets)
	@mkdir -p $(HOME)/.local/share/kryon/runtime/desktop
	@cp -r runtime/desktop/*.h $(HOME)/.local/share/kryon/runtime/desktop/ 2>/dev/null || true

	# Install third-party headers
	@mkdir -p $(HOME)/.local/share/kryon/third_party/stb
	@mkdir -p $(HOME)/.local/share/kryon/third_party/cJSON
	@mkdir -p $(HOME)/.local/share/kryon/third_party/tomlc99
	@cp third_party/stb/*.h $(HOME)/.local/share/kryon/third_party/stb/
	@cp third_party/cJSON/*.h $(HOME)/.local/share/kryon/third_party/cJSON/
	@cp third_party/tomlc99/toml.h $(HOME)/.local/share/kryon/third_party/tomlc99/

	# Install vendor files for web codegen
	@$(MAKE) -C $(CODEGENS_DIR)/web install-vendor 2>/dev/null || true

	@echo ""
	@echo "✓ Installation complete"
	@echo "  Binary: $(BINDIR)/kryon"
	@echo "  Libraries: $(LIBDIR)/"
	@echo "  Data: $(HOME)/.local/share/kryon/"

# ============================================================================
# Uninstall Target
# ============================================================================

uninstall:
	@echo "Uninstalling Kryon from $(PREFIX)..."
	@rm -f $(BINDIR)/kryon
	@rm -f $(LIBDIR)/libkryon_*
	@rm -rf $(INCLUDEDIR)
	@rm -rf $(HOME)/.local/share/kryon
	@echo "✓ Uninstalled from $(PREFIX)"

# ============================================================================
# Test Targets
# ============================================================================

test: all
	@echo "Running test suite..."
	@if [ -d $(TESTS_DIR) ]; then \
		$(MAKE) -C $(TESTS_DIR) run; \
	else \
		echo "No tests directory found"; \
	fi

check: all
	@echo "Running static analysis..."
	@echo "TODO: Add clang-tidy, cppcheck, etc."

# ============================================================================
# Documentation
# ============================================================================

docs:
	@echo "Generating documentation..."
	@echo "TODO: Add Doxygen, Sphinx, etc."

# ============================================================================
# Phony Targets
# ============================================================================

.PHONY: all cli ir dis codegens clean install uninstall
.PHONY: help test check docs

# ============================================================================
# Dependencies (Build Order)
# ============================================================================

# Ensure build directory exists
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
