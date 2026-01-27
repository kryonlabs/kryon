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
RENDERERS_DIR = renderers
RUNTIME_DIR = runtime
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
all: cli ir codegens runtime

# Help
help:
	@echo "Kryon Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all          - Build everything (default)"
	@echo "  cli          - Build CLI tool"
	@echo "  ir           - Build IR library"
	@echo "  codegens     - Build all code generators"
	@echo "  runtime      - Build runtime backends"
	@echo "  renderers    - Build rendering systems"
	@echo "  clean        - Remove all build artifacts"
	@echo "  install      - Install to $(PREFIX)"
	@echo ""
	@echo "Variants (use VARIANT=variant):"
	@echo "  VARIANT=terminal  - Terminal-only CLI (no SDL3)"
	@echo "  VARIANT=web       - Web-only CLI (no SDL3)"
	@echo "  VARIANT=desktop  - Full CLI with SDL3 (default)"
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

# Code generators (C, Kry, Kotlin, Markdown)
codegens: ir
	@echo "Building code generators..."
	@$(MAKE) -C $(CODEGENS_DIR)/c all
	@$(MAKE) -C $(CODEGENS_DIR)/kry all
	@$(MAKE) -C $(CODEGENS_DIR)/kotlin all
	@$(MAKE) -C $(CODEGENS_DIR)/markdown all
	@echo "✓ Built code generators"

# Runtime backends (desktop rendering, terminal, etc.)
runtime: ir
	@echo "Building rendering systems..."
	@if [ -d $(RENDERERS_DIR)/common ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/common all; \
	fi
	@if [ -d $(RENDERERS_DIR)/sdl3 ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/sdl3 all; \
	fi
	@echo "Building runtime backends..."
	@if [ -d $(RUNTIME_DIR)/desktop ]; then \
		$(MAKE) -C $(RUNTIME_DIR)/desktop all; \
	fi
	@echo "✓ Built runtime backends"

# Rendering systems (SDL3, common, etc.)
renderers: ir
	@echo "Building rendering systems..."
	@if [ -d $(RENDERERS_DIR)/common ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/common all; \
	fi
	@if [ -d $(RENDERERS_DIR)/sdl3 ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/sdl3 all; \
	fi
	@echo "✓ Built rendering systems"

# ============================================================================
# Clean Target
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR)
	@$(MAKE) -C $(IR_DIR) clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/c clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/kry clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/kotlin clean || true
	@$(MAKE) -C $(CODEGENS_DIR)/markdown clean || true
	@if [ -d $(RUNTIME_DIR)/desktop ]; then \
		$(MAKE) -C $(RUNTIME_DIR)/desktop clean || true; \
	fi
	@if [ -d $(RENDERERS_DIR)/common ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/common clean || true; \
	fi
	@if [ -d $(RENDERERS_DIR)/sdl3 ]; then \
		$(MAKE) -C $(RENDERERS_DIR)/sdl3 clean || true; \
	fi
	@$(MAKE) -C $(CLI_DIR) clean || true
	@echo "✓ Clean complete"

# ============================================================================
# Install Target
# ============================================================================

install: all
	@echo "Installing Kryon to $(PREFIX)..."
	@mkdir -p $(BINDIR) $(LIBDIR) $(INCLUDEDIR) $(PKGCONFIGDIR)
	@if [ -f $(CLI_DIR)/kryon ]; then \
		cp $(CLI_DIR)/kryon $(BINDIR)/; \
		echo "  Installed $(BINDIR)/kryon"; \
	fi
	@cp $(BUILD_DIR)/libkryon_ir.* $(LIBDIR)/ 2>/dev/null || true
	@cp -r ir/include/*.h $(INCLUDEDIR)/
	@echo "✓ Installed to $(PREFIX)"

# ============================================================================
# Uninstall Target
# ============================================================================

uninstall:
	@echo "Uninstalling Kryon from $(PREFIX)..."
	@rm -f $(BINDIR)/kryon
	@rm -f $(LIBDIR)/libkryon_*
	@rm -rf $(INCLUDEDIR)
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

.PHONY: all cli ir codegens runtime renderers clean install uninstall
.PHONY: help test check docs

# ============================================================================
# Dependencies (Build Order)
# ============================================================================

# Ensure build directory exists
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
