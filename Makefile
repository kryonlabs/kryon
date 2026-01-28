# Makefile for Kryon-C
# Usage:
#   make              - Build release version
#   make debug        - Build debug version
#   make clean        - Clean build artifacts
#   make run ARGS=... - Build and run with arguments

# Directories
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
BIN_DIR = $(BUILD_DIR)/bin
OBJ_DIR = $(BUILD_DIR)/obj
THIRD_PARTY_DIR = third-party

# Compiler and flags
CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -Wno-unused-variable -fPIC -D_GNU_SOURCE
LDFLAGS = -lm

# Build type (default: release)
DEBUG ?= 0
ifeq ($(DEBUG),1)
    CFLAGS += -g -O0 -DDEBUG
    BUILD_TYPE = debug
else
    CFLAGS += -O2 -DNDEBUG
    BUILD_TYPE = release
endif

# Detect SDL2 and Raylib using pkg-config
# If pkg-config is not available, leave the flags empty
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 SDL2_image SDL2_ttf fontconfig 2>/dev/null || echo "")
SDL2_LDFLAGS := $(shell pkg-config --libs sdl2 SDL2_image SDL2_ttf fontconfig 2>/dev/null || echo "")
RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null || echo "")
RAYLIB_LDFLAGS := $(shell pkg-config --libs raylib 2>/dev/null || echo "")

# Include paths - also use NIX_CFLAGS_COMPILE from nix shell
INCLUDES = -I$(INCLUDE_DIR) -I$(SRC_DIR) -I$(THIRD_PARTY_DIR)/cjson $(SDL2_CFLAGS) $(RAYLIB_CFLAGS) $(NIX_CFLAGS_COMPILE)

# Source files (grouped by module)
SHARED_SRC = $(SRC_DIR)/shared/kryon_mappings.c \
             $(SRC_DIR)/shared/krb_schema.c

CORE_SRC = $(SRC_DIR)/core/memory_manager.c \
           $(SRC_DIR)/core/binary_io.c \
           $(SRC_DIR)/core/arena.c \
           $(SRC_DIR)/core/color_utils.c \
           $(SRC_DIR)/core/error_system.c \
           $(SRC_DIR)/core/containers.c

COMPILER_SRC = $(SRC_DIR)/compiler/lexer/unicode.c \
               $(SRC_DIR)/compiler/lexer/lexer.c \
               $(SRC_DIR)/compiler/parser/parser.c \
               $(SRC_DIR)/compiler/parser/ast_utils.c \
               $(SRC_DIR)/compiler/diagnostics/diagnostics.c \
               $(SRC_DIR)/compiler/codegen/krb_file.c \
               $(SRC_DIR)/compiler/codegen/krb_writer.c \
               $(SRC_DIR)/compiler/codegen/krb_reader.c \
               $(SRC_DIR)/compiler/codegen/string_table.c \
               $(SRC_DIR)/compiler/codegen/ast_expression_serializer.c \
               $(SRC_DIR)/compiler/codegen/event_serializer.c \
               $(SRC_DIR)/compiler/codegen/element_serializer.c \
               $(SRC_DIR)/compiler/codegen/directive_serializer.c \
               $(SRC_DIR)/compiler/codegen/codegen.c \
               $(SRC_DIR)/compiler/codegen/ast_expander.c \
               $(SRC_DIR)/compiler/kir/kir_writer.c \
               $(SRC_DIR)/compiler/kir/kir_reader.c \
               $(SRC_DIR)/compiler/kir/kir_utils.c \
               $(SRC_DIR)/compiler/kir/kir_printer.c \
               $(SRC_DIR)/compiler/krb/krb_decompiler.c \
               $(SRC_DIR)/compiler/krl/krl_lexer.c \
               $(SRC_DIR)/compiler/krl/krl_parser.c \
               $(SRC_DIR)/compiler/krl/krl_to_kir.c

RUNTIME_SRC = $(SRC_DIR)/runtime/core/runtime.c \
              $(SRC_DIR)/runtime/core/validation.c \
              $(SRC_DIR)/runtime/core/component_state.c \
              $(SRC_DIR)/runtime/core/krb_loader.c \
              $(SRC_DIR)/runtime/core/state.c \
              $(SRC_DIR)/runtime/elements.c \
              $(SRC_DIR)/runtime/element_behaviors.c \
              $(SRC_DIR)/runtime/compilation/runtime_compiler.c \
              $(SRC_DIR)/runtime/behavior_integration.c \
              $(SRC_DIR)/runtime/expression_evaluator.c \
              $(SRC_DIR)/runtime/navigation/navigation.c \
              $(SRC_DIR)/runtime/elements/slider.c \
              $(SRC_DIR)/runtime/elements/container.c \
              $(SRC_DIR)/runtime/elements/dropdown.c \
              $(SRC_DIR)/runtime/elements/tabgroup.c \
              $(SRC_DIR)/runtime/elements/element_mixins.c \
              $(SRC_DIR)/runtime/elements/input.c \
              $(SRC_DIR)/runtime/elements/tab.c \
              $(SRC_DIR)/runtime/elements/tab_content.c \
              $(SRC_DIR)/runtime/elements/tabpanel.c \
              $(SRC_DIR)/runtime/elements/text.c \
              $(SRC_DIR)/runtime/elements/app.c \
              $(SRC_DIR)/runtime/elements/navigation_utils.c \
              $(SRC_DIR)/runtime/elements/button.c \
              $(SRC_DIR)/runtime/elements/checkbox.c \
              $(SRC_DIR)/runtime/elements/link.c \
              $(SRC_DIR)/runtime/elements/tabbar.c \
              $(SRC_DIR)/runtime/elements/image.c \
              $(SRC_DIR)/runtime/elements/grid.c

EVENTS_SRC = $(SRC_DIR)/events/events.c

# Detect optional renderers
RENDERERS_SRC =

# Check for raylib
ifeq ($(shell pkg-config --exists raylib && echo yes),yes)
    RENDERERS_SRC += $(SRC_DIR)/renderers/raylib/raylib_renderer.c
    RAYLIB_AVAILABLE := 1
    CFLAGS += -DKRYON_RENDERER_RAYLIB=1
else
    RAYLIB_AVAILABLE := 0
    $(warning Raylib not found - raylib renderer will be disabled)
endif

# Check for SDL2
ifeq ($(shell pkg-config --exists sdl2 && echo yes),yes)
    RENDERERS_SRC += $(SRC_DIR)/renderers/sdl2/sdl2_renderer.c
    SDL2_AVAILABLE := 1
    CFLAGS += -DKRYON_RENDERER_SDL2=1
    # Check if fontconfig is available
    ifeq ($(shell pkg-config --exists fontconfig && echo yes),yes)
        CFLAGS += -DUSE_FONTCONFIG=1
    endif
else
    SDL2_AVAILABLE := 0
    $(warning SDL2 not found - SDL2 renderer will be disabled)
endif

# Web renderer (always available - merged html + web)
RENDERERS_SRC += $(SRC_DIR)/renderers/html/html_renderer.c \
                $(SRC_DIR)/renderers/web/web_renderer.c \
                $(SRC_DIR)/renderers/web/dom_manager.c \
                $(SRC_DIR)/renderers/web/css_generator.c

CLI_SRC = $(SRC_DIR)/cli/main.c \
          $(SRC_DIR)/cli/run_command.c \
          $(SRC_DIR)/cli/compile_command.c \
          $(SRC_DIR)/cli/decompile_command.c \
          $(SRC_DIR)/cli/print_command.c \
          $(SRC_DIR)/cli/kir_commands.c \
          $(SRC_DIR)/cli/debug_command.c \
          $(SRC_DIR)/cli/package_command.c \
          $(SRC_DIR)/cli/dev_command.c

# =============================================================================
# PLATFORM SERVICES PLUGIN SYSTEM
# =============================================================================

# Services core (always included)
SERVICES_SRC = $(SRC_DIR)/services/services_registry.c \
               $(SRC_DIR)/services/services_null.c

# Plugin configuration
PLUGINS ?=
PLUGIN_SRC =
PLUGIN_OBJ =
PLUGIN_CFLAGS =
PLUGIN_LDFLAGS =
PLUGIN_INCLUDES =

# Auto-detect Inferno build environment
ifdef INFERNO
    # Building for Inferno emu - enable Inferno plugin
    PLUGINS += inferno
    $(info Detected INFERNO=$(INFERNO), enabling Inferno plugin)
endif

# Include plugin makefiles
ifneq ($(filter inferno,$(PLUGINS)),)
    include $(SRC_DIR)/plugins/inferno/Makefile
    $(info Building with Inferno platform services plugin)
endif

# Add plugin flags to compiler
CFLAGS += $(PLUGIN_CFLAGS)
INCLUDES += $(PLUGIN_INCLUDES)
LDFLAGS += $(PLUGIN_LDFLAGS)

# All source files
ALL_SRC = $(SHARED_SRC) $(CORE_SRC) $(COMPILER_SRC) $(RUNTIME_SRC) \
          $(EVENTS_SRC) $(RENDERERS_SRC) $(CLI_SRC) $(SERVICES_SRC) $(PLUGIN_SRC)

# Object files
ALL_OBJ = $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(ALL_SRC))

# Third-party objects
CJSON_OBJ = $(OBJ_DIR)/third-party/cjson/cjson.o

# Target binary
TARGET = $(BIN_DIR)/kryon

# Installation paths (can be overridden)
PREFIX ?= $(HOME)/.local
BINDIR ?= $(PREFIX)/bin
DATADIR ?= $(PREFIX)/share
COMPLETIONDIR ?= $(DATADIR)/bash-completion/completions
KRYON_DATADIR ?= $(DATADIR)/kryon

# Binary name
BINARY_NAME = kryon

# Default target
.PHONY: all
all: $(TARGET) completions/kryon.bash

# Create directories
$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@ $(sort $(dir $(ALL_OBJ))) $(sort $(dir $(PLUGIN_OBJ)))

# Build third-party cjson
$(OBJ_DIR)/third-party/cjson:
	@mkdir -p $@

$(CJSON_OBJ): $(THIRD_PARTY_DIR)/cjson/cJSON.c | $(OBJ_DIR)/third-party/cjson
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Compile source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Link binary
$(TARGET): $(ALL_OBJ) $(CJSON_OBJ) | $(BIN_DIR)
	@echo "[LD] $@"
	@$(CC) -o $@ $(ALL_OBJ) $(CJSON_OBJ) $(LDFLAGS) $(SDL2_LDFLAGS) $(RAYLIB_LDFLAGS) $(PLUGIN_LDFLAGS)
	@echo ""
	@echo "Build complete: $@"
	@echo "Build type: $(BUILD_TYPE)"
ifneq ($(PLUGINS),)
	@echo "Plugins: $(PLUGINS)"
endif

# Debug build
.PHONY: debug
debug: DEBUG=1
debug: clean all

# Clean build artifacts
.PHONY: clean
clean:
	@echo "[CLEAN] $(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)

# Run with arguments
.PHONY: run
run: $(TARGET)
	@$(TARGET) $(ARGS)

# List examples
.PHONY: list-examples
list-examples:
	@echo "Available examples:"
	@ls -1 examples/*.kry 2>/dev/null | xargs -n1 basename | sed 's/\.kry$$//' || echo "No examples found"

# Run example (usage: make run-example EXAMPLE=hello-world RENDERER=raylib)
.PHONY: run-example
run-example: $(TARGET)
	@if [ -z "$(EXAMPLE)" ]; then \
		echo "Usage: make run-example EXAMPLE=name RENDERER=renderer"; \
		echo "Run 'make list-examples' to see available examples"; \
		exit 1; \
	fi
	@if [ -z "$(RENDERER)" ]; then \
		RENDERER=raylib; \
	fi
	@$(TARGET) run examples/$(EXAMPLE).kry --renderer $(RENDERER)

# Show help
.PHONY: help
help:
	@echo "Kryon-C Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all (default)  - Build release version"
	@echo "  debug          - Build debug version"
	@echo "  clean          - Clean build artifacts"
	@echo "  run            - Build and run (usage: make run ARGS='...')"
	@echo "  list-examples  - List available examples"
	@echo "  run-example    - Run example (usage: make run-example EXAMPLE=hello-world RENDERER=raylib)"
	@echo "  install        - Install to ~/.local/bin (or PREFIX)"
	@echo "  uninstall      - Remove installation"
	@echo "  help           - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  DEBUG=1        - Build debug version"
	@echo "  ARGS=...       - Arguments for 'make run'"
	@echo "  EXAMPLE=name   - Example name for 'make run-example'"
	@echo "  RENDERER=name  - Renderer for 'make run-example' (default: raylib)"
	@echo "  PREFIX=path    - Installation prefix (default: ~/.local)"
	@echo "  DESTDIR=path   - Destination dir for packaging"

# Dependencies
-include $(ALL_OBJ:.o=.d)

# Generate dependencies (only for actual source files being compiled)
DEPS = $(ALL_OBJ:.o=.d)
$(DEPS): | $(OBJ_DIR)

$(OBJ_DIR)/%.d: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(INCLUDES) -MM -MT $(OBJ_DIR)/$*.o $< -MF $@

# Install target
.PHONY: install
install: $(TARGET)
	@echo "[INSTALL] Installing $(BINARY_NAME) to $(BINDIR)"
	@mkdir -p $(BINDIR)
	@install -m 0755 $(TARGET) $(BINDIR)/$(BINARY_NAME)
	@echo "[INSTALL] Installing completion to $(COMPLETIONDIR)"
	@mkdir -p $(COMPLETIONDIR)
	@install -m 0644 completions/kryon.bash $(COMPLETIONDIR)/kryon
	@echo "[INSTALL] Installing examples to $(KRYON_DATADIR)/examples"
	@mkdir -p $(KRYON_DATADIR)/examples
	@cp -r examples/* $(KRYON_DATADIR)/examples/
	@echo ""
	@echo "Installation complete!"
	@echo "  Binary: $(BINDIR)/$(BINARY_NAME)"
	@echo "  Completion: $(COMPLETIONDIR)/kryon"
	@echo "  Examples: $(KRYON_DATADIR)/examples/"
	@echo ""
	@echo "Make sure $(BINDIR) is in your PATH"

# Uninstall target
.PHONY: uninstall
uninstall:
	@echo "[UNINSTALL] Removing $(BINARY_NAME)"
	@rm -f $(BINDIR)/$(BINARY_NAME)
	@echo "[UNINSTALL] Removing completion"
	@rm -f $(COMPLETIONDIR)/kryon
	@echo "[UNINSTALL] Removing examples"
	@rm -rf $(KRYON_DATADIR)
	@echo "Uninstall complete"

.PHONY: all debug clean run list-examples run-example help install uninstall
