# Kryon UI Framework Build System
#
# This Makefile provides system-wide installation of Kryon CLI tool and library
# Supports both static and dynamic builds, with NixOS integration
#
# ARCHITECTURE:
# - Rendering backends: SDL3 (desktop), Terminal - use Kryon's renderer to draw UI
# - Transpilation frontends: HTML/web (/codegens/web/) - codegen that outputs browser-renderable code

.PHONY: all clean install install-dynamic install-static uninstall doctor dev test help
.PHONY: build-cli build-lib build-dynamic build-static
.PHONY: build-terminal build-desktop build-web build-default build-all-variants
.PHONY: test-serialization test-validation test-conversion test-backend test-integration test-all test-modular
.PHONY: generate-examples validate-examples clean-generated
.PHONY: build-android clean-android install-android check-android-ndk

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

# Static build flags (includes all rendering backends + transpilers)
STATIC_FLAGS = -d:staticBackend --opt:size --passL:"-static"

# Dynamic build flags (uses system libraries)
# Add rpath so the CLI can find libraries in ~/.local/lib at runtime
DYNAMIC_FLAGS = --opt:speed --passL:"-Wl,-rpath,$(LIBDIR) -L$(LIBDIR)"

# SDL3 Configuration
SDL3_CFLAGS = $(shell pkg-config --cflags sdl3 2>/dev/null || echo "-I/usr/include/SDL3")
SDL3_LIBS = $(shell pkg-config --libs sdl3 2>/dev/null || echo "-lSDL3")
SDL_TTF_LIBS = $(shell pkg-config --libs SDL3_ttf 2>/dev/null || echo "-lSDL3_ttf")

# Raylib Configuration
RAYLIB_CFLAGS = $(shell pkg-config --cflags raylib 2>/dev/null || echo "-I/usr/local/include")
RAYLIB_LIBS = $(shell pkg-config --libs raylib 2>/dev/null || echo "-lraylib -lm")

# Target-specific build flags (lightweight variants)
TERMINAL_FLAGS = -d:terminalOnly --opt:size        # Terminal renderer only
DESKTOP_FLAGS = -d:desktopBackend --opt:speed      # SDL3 renderer only
RAYLIB_FLAGS = -d:raylibBackend --opt:speed        # Raylib renderer only (3D support)
WEB_FLAGS = -d:webBackend --opt:size               # HTML/web transpiler only (not a renderer)

# Detect NixOS environment
ifeq ($(shell test -e /etc/nixos && echo yes), yes)
    NIXOS = 1
    STATIC_FLAGS += --passL:"-L$(NIX_CC_TARGET_PREFIX)/lib"
endif

# Source directories
CLI_DIR = cli
CLI_SRC = $(CLI_DIR)/src/main.c
CLI_DEPS = $(wildcard $(CLI_DIR)/src/**/*.c)
LIB_SRC = bindings/nim/kryon_dsl

# Build targets
BUILD_DIR = build
BIN_DIR = bin
CLI_BIN = $(BUILD_DIR)/kryon
STATIC_BIN = $(BUILD_DIR)/kryon-static
DYNAMIC_BIN = $(BUILD_DIR)/kryon-dynamic
LIB_FILE = $(BUILD_DIR)/libkryon.a

# Renderer-specific binaries
TERMINAL_BIN = $(BUILD_DIR)/kryon-terminal
DESKTOP_BIN = $(BUILD_DIR)/kryon-desktop
RAYLIB_BIN = $(BUILD_DIR)/kryon-raylib
WEB_BIN = $(BUILD_DIR)/kryon-web
DEFAULT_BIN = $(BUILD_DIR)/kryon-full

# Default target
all: build-cli build-lib

# Build CLI tool (development version)
build-cli: $(CLI_BIN)

$(CLI_BIN): $(CLI_DEPS) build-codegens
	@echo "Building Kryon CLI (development - C version)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(CLI_DIR) clean
	$(MAKE) -C $(CLI_DIR)
	@cp $(CLI_DIR)/kryon $(CLI_BIN)

# Build static CLI with all backends bundled (C version)
build-static: $(STATIC_BIN)

$(STATIC_BIN): $(CLI_DEPS)
	@echo "Building Kryon CLI (C version with static linking)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(CLI_DIR) clean
	$(MAKE) -C $(CLI_DIR)
	@cp $(CLI_DIR)/kryon $(STATIC_BIN)

# Build dynamic CLI using system libraries (C version)
build-dynamic: $(DYNAMIC_BIN)

$(DYNAMIC_BIN): $(CLI_DEPS)
	@echo "Building Kryon CLI (C version with dynamic linking)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C $(CLI_DIR) clean
	$(MAKE) -C $(CLI_DIR)
	@cp $(CLI_DIR)/kryon $(DYNAMIC_BIN)

# =============================================================================
# Renderer-specific builds (for GitHub Actions CI)
# =============================================================================

# Terminal-only CLI (no SDL3 dependencies - lightweight)
build-terminal: $(TERMINAL_BIN)

$(TERMINAL_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (terminal-only)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp; fi
	$(NIM) c $(NIMFLAGS) $(TERMINAL_FLAGS) \
		--passL:"-L$(BUILD_DIR) -lkryon_ir" \
		-o:$(TERMINAL_BIN) $(CLI_SRC)
	@if [ -f nim.cfg.tmp ]; then mv nim.cfg.tmp nim.cfg; fi

# Desktop CLI (requires SDL3)
build-desktop: $(DESKTOP_BIN)

$(DESKTOP_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (desktop/SDL3)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	$(MAKE) -C backends/desktop all
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp; fi
	$(NIM) c $(NIMFLAGS) $(DESKTOP_FLAGS) \
		--passL:"-L$(BUILD_DIR) -lkryon_ir -lkryon_desktop $(SDL3_LIBS) $(SDL_TTF_LIBS)" \
		-o:$(DESKTOP_BIN) $(CLI_SRC)
	@if [ -f nim.cfg.tmp ]; then mv nim.cfg.tmp nim.cfg; fi

# Raylib CLI (requires raylib - 3D rendering support)
build-raylib: $(RAYLIB_BIN)

$(RAYLIB_BIN): $(CLI_SRC) $(LIB_FILE)
	@echo "Building Kryon CLI (raylib/3D)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	$(MAKE) -C renderers/common all
	$(MAKE) -C renderers/common install
	$(MAKE) -C renderers/raylib all
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp; fi
	$(NIM) c $(NIMFLAGS) $(RAYLIB_FLAGS) \
		--passL:"-L$(BUILD_DIR) -lkryon_raylib -lkryon_common -lkryon_ir -lkryon $(RAYLIB_LIBS)" \
		-o:$(RAYLIB_BIN) $(CLI_SRC)
	@if [ -f nim.cfg.tmp ]; then mv nim.cfg.tmp nim.cfg; fi

# Web CLI (HTML/CSS/JS transpiler - codegen, not a renderer)
build-web: $(WEB_BIN)

$(WEB_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (HTML/web transpiler)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	$(MAKE) -C codegens/web all
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp; fi
	$(NIM) c $(NIMFLAGS) $(WEB_FLAGS) \
		--passL:"-L$(BUILD_DIR) -lkryon_ir -lkryon_web" \
		-o:$(WEB_BIN) $(CLI_SRC)
	@if [ -f nim.cfg.tmp ]; then mv nim.cfg.tmp nim.cfg; fi

# Default CLI (all targets: renderers + transpilers)
build-default: $(DEFAULT_BIN)

$(DEFAULT_BIN): $(CLI_SRC)
	@echo "Building Kryon CLI (all targets: renderers + transpilers)..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	$(MAKE) -C backends/desktop all
	$(MAKE) -C renderers/raylib all
	$(MAKE) -C codegens/web all
	@if [ -f nim.cfg ]; then mv nim.cfg nim.cfg.tmp; fi
	$(NIM) c $(NIMFLAGS) -d:staticBackend --opt:speed \
		--passL:"-L$(BUILD_DIR) -lkryon_ir -lkryon_desktop -lkryon_raylib -lkryon_web $(SDL3_LIBS) $(SDL_TTF_LIBS) $(RAYLIB_LIBS)" \
		-o:$(DEFAULT_BIN) $(CLI_SRC)
	@if [ -f nim.cfg.tmp ]; then mv nim.cfg.tmp nim.cfg; fi

# Build all renderer variants
build-all-variants: build-terminal build-desktop build-raylib build-web build-default
	@echo "✅ All renderer variants built successfully!"

# Build library for use by other projects
# This creates a combined archive with all C libraries needed for linking
build-lib: build-c-libs $(LIB_FILE)

# Build the C core libraries
build-c-libs:
	@echo "Building C core libraries..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C ir all
	$(MAKE) -C renderers/common all
	$(MAKE) -C renderers/common install
	$(MAKE) -C renderers/sdl3 all
	$(MAKE) -C backends/desktop all

# Build code generators (needed by CLI)
build-codegens:
	@echo "Building code generators..."
	@mkdir -p $(BUILD_DIR)
	$(MAKE) -C codegens all
	@for dir in kry tsx nim lua markdown; do \
		if [ -f codegens/$$dir/Makefile ]; then \
			$(MAKE) -C codegens/$$dir all || true; \
		fi \
	done

# Create combined static library from all C libraries
$(LIB_FILE): build-c-libs
	@echo "Creating combined Kryon library..."
	@mkdir -p $(BUILD_DIR)
	@# Create a temporary directory for combining archives
	@rm -rf $(BUILD_DIR)/combined_lib
	@mkdir -p $(BUILD_DIR)/combined_lib
	@# Extract all object files from component libraries
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

# Install dynamic version (default for most users) - C CLI only
install-dynamic: build-dynamic
	@echo "Installing Kryon CLI (C version)..."
	@mkdir -p $(BINDIR)
	@mkdir -p $(LIBDIR)
	install -m 755 $(DYNAMIC_BIN) $(BINDIR)/kryon
	@echo "✓ Installed C CLI to $(BINDIR)/kryon"

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
	# Install shared libraries if they exist
	@if [ -f build/libkryon_ir.so ]; then \
		install -m 755 build/libkryon_ir.so $(LIBDIR)/; \
		echo "✓ Installed libkryon_ir.so"; \
	fi
	@if [ -f build/libkryon_desktop.so ]; then \
		install -m 755 build/libkryon_desktop.so $(LIBDIR)/; \
		echo "✓ Installed libkryon_desktop.so"; \
	fi
	@if [ -f build/libkryon_web.so ]; then \
		install -m 755 build/libkryon_web.so $(LIBDIR)/; \
		echo "✓ Installed libkryon_web.so"; \
	fi
	# Copy Nim bindings to include directory (optional - only if they exist)
	@mkdir -p $(INCDIR)
	@if [ -d bindings/nim ] && [ -n "$$(ls -A bindings/nim 2>/dev/null)" ]; then \
		cp -r bindings/nim/* $(INCDIR)/ 2>/dev/null || true; \
		echo "✓ Installed Nim bindings"; \
	fi
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

# Modular test targets for IR pipeline
test-serialization:
	@echo "Running serialization module tests..."
	@if [ -n "$$(ls -A tests/serialization/*.c 2>/dev/null)" ]; then \
		for test in tests/serialization/*.c; do \
			echo "  - Running $$test"; \
			gcc $$test -o /tmp/test_serial -Iir -Lbuild -lkryon_ir && /tmp/test_serial || exit 1; \
		done; \
	else \
		echo "  ⚠️  No serialization tests found (placeholder directory)"; \
	fi

test-validation:
	@echo "Running validation module tests..."
	@if [ -n "$$(ls -A tests/validation/*.c 2>/dev/null)" ]; then \
		for test in tests/validation/*.c; do \
			echo "  - Running $$test"; \
			gcc $$test -o /tmp/test_valid -Iir -Lbuild -lkryon_ir && /tmp/test_valid || exit 1; \
		done; \
	else \
		echo "  ⚠️  No validation tests found (placeholder directory)"; \
	fi

test-conversion:
	@echo "Running conversion module tests (.kir → .kirb)..."
	@if [ -n "$$(ls -A tests/conversion/*.c 2>/dev/null)" ]; then \
		for test in tests/conversion/*.c; do \
			echo "  - Running $$test"; \
			gcc $$test -o /tmp/test_conv -Iir -Lbuild -lkryon_ir && /tmp/test_conv || exit 1; \
		done; \
	else \
		echo "  ⚠️  No conversion tests found (placeholder directory)"; \
	fi

test-backend:
	@echo "Running backend module tests..."
	@if [ -n "$$(ls -A tests/backend/*.c 2>/dev/null)" ]; then \
		for test in tests/backend/*.c; do \
			echo "  - Running $$test"; \
			gcc $$test -o /tmp/test_backend -Iir -Ibackends/desktop -Lbuild -lkryon_desktop -lkryon_ir && /tmp/test_backend || exit 1; \
		done; \
	else \
		echo "  ⚠️  No backend tests found (placeholder directory)"; \
	fi

test-integration:
	@echo "Running integration tests (end-to-end pipeline)..."
	@if [ -n "$$(ls -A tests/integration/*.c 2>/dev/null)" ]; then \
		for test in tests/integration/*.c; do \
			echo "  - Running $$test"; \
			gcc $$test -o /tmp/test_integ -Iir -Ibackends/desktop -Lbuild -lkryon_desktop -lkryon_ir && /tmp/test_integ || exit 1; \
		done; \
	else \
		echo "  ⚠️  No integration tests found (placeholder directory)"; \
	fi

test-modular: test-serialization test-validation test-conversion test-backend test-integration
	@echo "✅ All modular tests passed!"

test-all: test test-modular
	@echo "✅ Complete test suite passed!"

# ============================================================================
# ASAN (AddressSanitizer) Build and Testing
# ============================================================================

.PHONY: asan-full build-cli-asan asan-test clean-asan

# Build entire stack with AddressSanitizer and UndefinedBehaviorSanitizer
asan-full:
	@echo "🔍 Building Kryon with ASAN+UBSan (full stack)..."
	@echo "Step 1/3: Building IR library with ASAN+UBSan..."
	$(MAKE) -C ir clean
	$(MAKE) -C ir asan-ubsan
	@echo "Step 2/3: Building Desktop backend with ASAN+UBSan..."
	$(MAKE) -C backends/desktop clean
	$(MAKE) -C backends/desktop asan-ubsan
	@echo "Step 3/3: Building Nim CLI with ASAN-instrumented libraries..."
	@ASAN_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
		$(MAKE) build-cli-asan
	@echo "✅ ASAN build complete!"
	@echo ""
	@echo "Usage: LD_LIBRARY_PATH=build:\$$LD_LIBRARY_PATH ./run_example.sh <example>"
	@echo "       ASAN_OPTIONS=detect_leaks=1 LD_LIBRARY_PATH=build:\$$LD_LIBRARY_PATH ./run_example.sh <example>"

# Build Nim CLI with ASAN-instrumented libraries
build-cli-asan: $(CLI_SRC)
	@mkdir -p $(BUILD_DIR)
	$(NIM) c $(NIMFLAGS) --opt:speed \
		--passL:"-fsanitize=address,undefined -fno-omit-frame-pointer -L$(BUILD_DIR) -lkryon_ir -lkryon_desktop $(SDL3_LIBS) $(SDL_TTF_LIBS)" \
		-o:$(CLI_BIN) $(CLI_SRC)

# Run ASAN test on example
asan-test:
	@if [ -z "$(EXAMPLE)" ]; then \
		echo "Usage: make asan-test EXAMPLE=<example_name>"; \
		echo "Example: make asan-test EXAMPLE=button_demo"; \
		exit 1; \
	fi
	@echo "🧪 Running ASAN test on $(EXAMPLE)..."
	@ASAN_OPTIONS="detect_leaks=1:halt_on_error=0:log_path=/tmp/asan.log" \
		LD_LIBRARY_PATH=build:\$$LD_LIBRARY_PATH \
		timeout 5 ./run_example.sh $(EXAMPLE) 2>&1 | tee /tmp/asan_output.txt || true
	@if grep -q "ERROR: AddressSanitizer" /tmp/asan_output.txt 2>/dev/null; then \
		echo "❌ ASAN errors detected!"; \
		if [ -f ./scripts/parse_asan.sh ]; then \
			./scripts/parse_asan.sh /tmp/asan_output.txt; \
		fi; \
		exit 1; \
	else \
		echo "✅ No ASAN errors detected"; \
	fi

# Clean ASAN build artifacts
clean-asan:
	@echo "🧹 Cleaning ASAN build artifacts..."
	$(MAKE) -C ir clean
	$(MAKE) -C backends/desktop clean
	rm -f $(CLI_BIN)
	rm -f /tmp/asan*.log /tmp/asan_output.txt
	@echo "✅ ASAN clean complete!"

# Interactive testing
.PHONY: test-interactive test-all-with-interactive

test-interactive: build install
	@echo "🧪 Running interactive tests..."
	@./scripts/run_interactive_tests.sh tests/interactive

test-all-with-interactive: test test-modular test-interactive
	@echo "✅ Complete test suite (including interactive) passed!"

# Examples
examples:
	@echo "Building examples..."
	$(MAKE) -C examples

# Generate examples from .kry source files
generate-examples:
	@echo "Generating examples from .kry files..."
	@./scripts/generate_examples.sh

# Validate round-trip transpilation (all examples)
validate-examples:
	@echo "Validating round-trip transpilation..."
	@./scripts/generate_examples.sh --validate

# Clean generated example files
clean-generated:
	@echo "Cleaning generated examples..."
	@./scripts/generate_examples.sh --clean
	@rm -rf examples/nim/ examples/lua/

# Generate plugin bindings
generate-bindings:
	@echo "Generating plugin bindings..."
	@if [ -z "$(PLUGIN)" ]; then \
		echo "Usage: make generate-bindings PLUGIN=<plugin_name>"; \
		echo "Example: make generate-bindings PLUGIN=canvas"; \
		exit 1; \
	fi
	@if [ ! -f "plugins/$(PLUGIN)/bindings.json" ]; then \
		echo "Error: plugins/$(PLUGIN)/bindings.json not found"; \
		exit 1; \
	fi
	@echo "Compiling bindings generator..."
	@$(NIM) c --hints:off --warnings:off -o:build/generate_bindings tools/generate_nim_bindings.nim
	@echo "Generating Nim bindings for $(PLUGIN)..."
	@build/generate_bindings plugins/$(PLUGIN)/bindings.json bindings/nim/$(PLUGIN)_generated.nim
	@echo "✓ Bindings generated: bindings/nim/$(PLUGIN)_generated.nim"

# Clean build artifacts
# ============================================================================
# Android Platform Targets
# ============================================================================

# Build Android platform library for all ABIs
build-android:
	@echo "Building Android platform library..."
	$(MAKE) -C platforms/android ndk-build

# Clean Android build artifacts
clean-android:
	@echo "Cleaning Android build artifacts..."
	$(MAKE) -C platforms/android clean

# Check Android NDK installation
check-android-ndk:
	@echo "Checking Android NDK installation..."
	@$(MAKE) -C platforms/android check-ndk

# ============================================================================
# Clean Targets
# ============================================================================

clean:
	@echo "Cleaning build artifacts..."
	rm -rf $(BUILD_DIR)
	rm -rf $(BIN_DIR)
	rm -rf nimcache/
	@echo "Cleaning Android native builds..."
	@rm -rf renderers/android/build
	@rm -rf backends/android/build
	@rm -rf bindings/kotlin/build
	@echo "Cleaning Android project builds..."
	@find /tmp -maxdepth 1 -name "kryon_android_*" -type d -exec rm -rf {}/build {}/app/build {}/.gradle \; 2>/dev/null || true

# Deep clean (including installation artifacts and Android)
distclean: clean clean-android
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
	@echo "  build-static  Build static CLI with all targets bundled"
	@echo "  build-dynamic Build dynamic CLI using system libraries"
	@echo "  build-lib     Build library for other projects"
	@echo "  dev           Development build with debug symbols"
	@echo ""
	@echo "Target-specific builds (for CI):"
	@echo "  build-terminal      Terminal renderer only"
	@echo "  build-desktop       SDL3 renderer only"
	@echo "  build-web           HTML/web transpiler (codegen, not a renderer)"
	@echo "  build-default       Full CLI with all targets (renderers + transpilers)"
	@echo "  build-all-variants  Build all target variants"
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
	@echo ""
	@echo "Android Platform:"
	@echo "  build-android       Build Android platform library (all ABIs)"
	@echo "  clean-android       Clean Android build artifacts"
	@echo "  check-android-ndk   Verify Android NDK installation"
	@echo "  generate-examples    Generate all examples from .kry files"
	@echo "  validate-examples    Validate round-trip transpilation"
	@echo "  clean-generated      Clean generated example files"
	@echo "  generate-bindings PLUGIN=<name>  Generate plugin bindings"
	@echo "  clean         Clean build artifacts"
	@echo "  distclean     Deep clean including generated files"
	@echo "  help          Show this help"
	@echo ""
	@echo "Memory Debugging (ASAN):"
	@echo "  asan-full     Build entire stack with AddressSanitizer+UBSan"
	@echo "  asan-test EXAMPLE=<name>  Run ASAN test on specific example"
	@echo "  clean-asan    Clean ASAN build artifacts"
	@echo ""
	@echo "Installation paths:"
	@echo "  CLI: $(BINDIR)/kryon"
	@echo "  Library: $(LIBDIR)/libkryon.a"
	@echo "  Headers: $(INCDIR)/"
	@echo "  Config: $(CONFIGDIR)/"