# Kryon UI Framework Build System
#
# This Makefile provides system-wide installation of Kryon CLI tool and library
# Supports both static and dynamic builds, with NixOS integration

.PHONY: all clean install install-dynamic install-static uninstall doctor dev test help
.PHONY: build-cli build-lib build-dynamic build-static

# Configuration
VERSION = 0.2.0
PREFIX = $(HOME)/.local
BINDIR = $(PREFIX)/bin
LIBDIR = $(PREFIX)/lib
INCDIR = $(PREFIX)/include/kryon
PKGCONFIGDIR = $(PREFIX)/lib/pkgconfig
CONFIGDIR = $(HOME)/.config/kryon

# Compiler and flags
NIM = nim
NIMFLAGS = --define:kryonVersion=$(VERSION)

# Static build flags (includes all backends)
STATIC_FLAGS = -d:staticBackend --opt:size --passL:"-static"

# Dynamic build flags (uses system libraries)
DYNAMIC_FLAGS = --opt:speed

# Detect NixOS environment
ifeq ($(shell test -e /etc/nixos && echo yes), yes)
    NIXOS = 1
    STATIC_FLAGS += --passL:"-L$(NIX_CC_TARGET_PREFIX)/lib"
endif

# Source directories
CLI_SRC = cli/main.nim
LIB_SRC = bindings/nim/kryon_dsl

# Build targets
BUILD_DIR = build
BIN_DIR = bin
CLI_BIN = $(BUILD_DIR)/kryon
STATIC_BIN = $(BUILD_DIR)/kryon-static
DYNAMIC_BIN = $(BUILD_DIR)/kryon-dynamic
LIB_FILE = $(BUILD_DIR)/libkryon.a

# Default target
all: build-cli build-lib

# Build CLI tool (development version)
build-cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (development)..."
	@mkdir -p $(BUILD_DIR)
	@# Backup nim.cfg if it exists and build without C dependencies
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp && \
		$(NIM) c $(NIMFLAGS) --opt:speed -o:$(CLI_BIN) $(CLI_SRC) && \
		mv nim.cfg.tmp nim.cfg; \
	else \
		$(NIM) c $(NIMFLAGS) --opt:speed -o:$(CLI_BIN) $(CLI_SRC); \
	fi

# Build static CLI with all backends bundled
build-static: $(STATIC_BIN)

$(STATIC_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (static with all backends)..."
	@mkdir -p $(BUILD_DIR)
	$(NIM) c $(NIMFLAGS) $(STATIC_FLAGS) -o:$(STATIC_BIN) $(CLI_SRC)

# Build dynamic CLI using system libraries
build-dynamic: $(DYNAMIC_BIN)

$(DYNAMIC_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (dynamic linking)..."
	@mkdir -p $(BUILD_DIR)
	@# Build without C dependencies
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp && \
		$(NIM) c $(NIMFLAGS) $(DYNAMIC_FLAGS) -o:$(DYNAMIC_BIN) $(CLI_SRC) && \
		mv nim.cfg.tmp nim.cfg; \
	else \
		$(NIM) c $(NIMFLAGS) $(DYNAMIC_FLAGS) -o:$(DYNAMIC_BIN) $(CLI_SRC); \
	fi

# Build library for use by other projects
# This creates a combined archive with all C libraries needed for linking
build-lib: build-c-libs $(LIB_FILE)

# Build the C core libraries
build-c-libs:
	@echo "Building C core libraries..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C core all
	$(MAKE) -C ir all
	$(MAKE) -C backends/desktop all

# Create combined static library from all C libraries
$(LIB_FILE): build-c-libs
	@echo "Creating combined Kryon library..."
	@mkdir -p $(BUILD_DIR)
	@# Create a temporary directory for combining archives
	@rm -rf $(BUILD_DIR)/combined_lib
	@mkdir -p $(BUILD_DIR)/combined_lib
	@# Extract all object files from component libraries
	@cd $(BUILD_DIR)/combined_lib && ar x ../libkryon_core.a
	@cd $(BUILD_DIR)/combined_lib && ar x ../libkryon_ir.a
	@cd $(BUILD_DIR)/combined_lib && ar x ../libkryon_desktop.a
	@# Create combined archive
	ar rcs $(LIB_FILE) $(BUILD_DIR)/combined_lib/*.o
	@rm -rf $(BUILD_DIR)/combined_lib
	@echo "Created $(LIB_FILE)"

# Development build (debug symbols, verbose)
dev:
	@echo "Building development version..."
	$(NIM) c $(NIMFLAGS) -d:debug -d:verbose --opt:none -o:$(CLI_BIN) $(CLI_SRC)

# Installation targets
install: install-dynamic install-lib install-config
	@echo "Installation complete!"
	@echo "CLI: $(BINDIR)/kryon"
	@echo "Library: $(LIBDIR)/libkryon.a"
	@echo "Headers: $(INCDIR)/"

# Install dynamic version (default for most users)
install-dynamic:
	@echo "Installing Kryon CLI (dynamic)..."
	@mkdir -p $(BINDIR)
	@mkdir -p $(LIBDIR)
	# Try the normal build first, but if it fails, use the working CLI
	@if $(MAKE) build-dynamic >/dev/null 2>&1 && [ -f $(DYNAMIC_BIN) ]; then \
		install -m 755 $(DYNAMIC_BIN) $(BINDIR)/kryon; \
		echo "✓ Installed dynamic CLI"; \
	else \
		echo "Building CLI without C dependencies (fallback)..."; \
		mv nim.cfg nim.cfg.backup 2>/dev/null || true; \
		if nim compile --out:$(BINDIR)/kryon cli/main.nim; then \
			echo "✓ Installed CLI without C dependencies"; \
		else \
			echo "✗ Failed to build CLI"; \
			exit 1; \
		fi; \
		mv nim.cfg.backup nim.cfg 2>/dev/null || true; \
	fi

# Install static version (self-contained)
install-static: build-static
	@echo "Installing Kryon CLI (static)..."
	@mkdir -p $(BINDIR)
	install -m 755 $(STATIC_BIN) $(BINDIR)/kryon

# Install library for development
install-lib: build-lib
	@echo "Installing Kryon library..."
	@mkdir -p $(LIBDIR)
	@mkdir -p $(PKGCONFIGDIR)
	install -m 644 $(LIB_FILE) $(LIBDIR)/
	# Copy Nim bindings to include directory
	rm -rf $(INCDIR)
	mkdir -p $(INCDIR)
	cp -r bindings/nim/* $(INCDIR)/
	cp c_core_build.nim $(PREFIX)/include/
	# Copy C headers for compilation (fix relative paths for flat install)
	mkdir -p $(INCDIR)/c
	cp core/include/*.h $(INCDIR)/c/
	cp ir/*.h $(INCDIR)/c/
	for h in backends/desktop/*.h; do \
		sed 's|#include "../../ir/|#include "|g' "$$h" > $(INCDIR)/c/$$(basename "$$h"); \
	done
	# Create pkg-config file if it exists
	@if [ -f kryon.pc ]; then \
		sed -e 's/@PREFIX@/$(subst /,\/,$(PREFIX))/g' \
		    -e 's/@VERSION@/$(VERSION)/g' \
		    kryon.pc > $(PKGCONFIGDIR)/kryon.pc; \
	fi

# Install configuration and templates
install-config:
	@echo "Installing configuration..."
	@mkdir -p $(CONFIGDIR)
	@mkdir -p $(CONFIGDIR)/templates
	cp -r templates/* $(CONFIGDIR)/templates/ 2>/dev/null || true
	echo "version: $(VERSION)" > $(CONFIGDIR)/config.yaml
	echo "installed_at: $(shell date)" >> $(CONFIGDIR)/config.yaml

# System-wide installation (requires sudo)
install-system: PREFIX = /usr/local
install-system: install

# Uninstallation
uninstall:
	@echo "Removing Kryon installation..."
	rm -f $(BINDIR)/kryon
	rm -f $(LIBDIR)/libkryon.a
	rm -rf $(INCDIR)
	rm -f $(PKGCONFIGDIR)/kryon.pc
	rm -rf $(CONFIGDIR)
	@echo "Uninstallation complete!"

# Health check and dependency verification
doctor:
	@echo "Kryon System Health Check"
	@echo "========================="
	@echo ""
	@echo "Version: $(VERSION)"
	@echo "Prefix: $(PREFIX)"
	@echo "Build: $(shell uname -s)-$(shell uname -m)"
	@echo ""
	@echo "Dependencies:"
	@if command -v $(NIM) >/dev/null 2>&1; then \
		echo "✓ Nim: $$($(NIM) --version | head -n1)"; \
	else \
		echo "✗ Nim: not found"; \
	fi
	@if [ -f $(LIBDIR)/libkryon.a ]; then \
		echo "✓ Library: $(LIBDIR)/libkryon.a"; \
	else \
		echo "✗ Library: not installed"; \
	fi
	@if [ -f $(BINDIR)/kryon ]; then \
		echo "✓ CLI: $(BINDIR)/kryon"; \
	else \
		echo "✗ CLI: not installed"; \
	fi
	@echo ""
	@echo "Backend Libraries:"
	@if pkg-config --exists raylib 2>/dev/null || nimble list raylib | grep -q raylib; then \
		echo "✓ Raylib: available"; \
	else \
		echo "✗ Raylib: not found (install with: nimble install raylib)"; \
	fi
	@if pkg-config --exists sdl2 2>/dev/null; then \
		echo "✓ SDL2: available"; \
	else \
		echo "✗ SDL2: not found (system package required)"; \
	fi
	@echo ""
	@if [ -f $(CONFIGDIR)/config.yaml ]; then \
		echo "Configuration: $(CONFIGDIR)/config.yaml"; \
	else \
		echo "Configuration: not created yet"; \
	fi

# Testing
test:
	@echo "Running tests..."
	$(NIM) c -r tests/test_all.nim

# Examples
examples:
	@echo "Building examples..."
	$(MAKE) -C examples

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	rm -rf nimcache/

# Deep clean (including installation artifacts)
distclean: clean
	@echo "Deep cleaning..."
	rm -f $(LIB_FILE)
	rm -f *.pc

# Show help
help:
	@echo "Kryon Build System"
	@echo "=================="
	@echo ""
	@echo "Build targets:"
	@echo "  all           Build CLI and library (development)"
	@echo "  build-cli     Build CLI tool only"
	@echo "  build-static  Build static CLI with backends bundled"
	@echo "  build-dynamic Build dynamic CLI using system libraries"
	@echo "  build-lib     Build library for other projects"
	@echo "  dev           Development build with debug symbols"
	@echo ""
	@echo "Installation:"
	@echo "  install       Install dynamic CLI, library and config"
	@echo "  install-static Install static CLI (self-contained)"
	@echo "  install-dynamic Install dynamic CLI (uses system libs)"
	@echo "  install-lib   Install library for development"
	@echo "  install-system System-wide installation (requires sudo)"
	@echo "  uninstall     Remove all installed files"
	@echo ""
	@echo "Utilities:"
	@echo "  doctor        System health check and dependencies"
	@echo "  test          Run test suite"
	@echo "  examples      Build example programs"
	@echo "  clean         Clean build artifacts"
	@echo "  distclean     Deep clean including generated files"
	@echo "  help          Show this help"
	@echo ""
	@echo "Installation paths:"
	@echo "  CLI: $(BINDIR)/kryon"
	@echo "  Library: $(LIBDIR)/libkryon.a"
	@echo "  Headers: $(INCDIR)/"
	@echo "  Config: $(CONFIGDIR)/"