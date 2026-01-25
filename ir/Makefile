# Makefile for Kryon IR System
# Compiles the core IR library that can be used by frontends and backends

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -Iinclude -I../include
AR = ar
ARFLAGS = rcs

# Single-pass layout system (ONLY system - no legacy code)
CFLAGS += -DIR_USE_SINGLE_PASS_LAYOUT

# HarfBuzz, FreeType, and FriBidi for text shaping
# These must be passed in from the environment or overridden on command line
HARFBUZZ_CFLAGS ?= $(shell pkg-config --cflags harfbuzz freetype2 fribidi 2>/dev/null || echo "")
HARFBUZZ_LIBS ?= $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || echo "")

# Check if harfbuzz is available - REQUIRED, no fallbacks
HAS_HARFBUZZ = $(shell pkg-config --exists harfbuzz && echo 1 || echo 0)
HAS_FRIBIDI = $(shell pkg-config --exists fribidi && echo 1 || echo 0)

ifeq ($(HAS_HARFBUZZ),0)
$(error HarfBuzz is REQUIRED. Install with: apt install libharfbuzz-dev OR nix-shell)
endif

ifeq ($(HAS_FRIBIDI),0)
$(error FriBidi is REQUIRED. Install with: apt install libfribidi-dev OR nix-shell)
endif

# Text shaping object file - no fallback, dependencies are required
IR_TEXT_SHAPING_OBJ = src/features/text_shaping.o
IR_TEXT_SHAPING_LIBS = $(HARFBUZZ_LIBS)

# Directories
IR_DIR = .
BUILD_DIR = ../build
INCLUDE_DIR = include
SRC_DIR = src

# Third-party directories
THIRD_PARTY_DIR = ../third_party
CJSON_DIR = $(THIRD_PARTY_DIR)/cJSON
STB_DIR = $(THIRD_PARTY_DIR)/stb
TOML_DIR = $(THIRD_PARTY_DIR)/tomlc99

# Third-party include paths
CFLAGS += -I$(CJSON_DIR) -I$(STB_DIR) -I$(TOML_DIR)
CFLAGS += $(HARFBUZZ_CFLAGS)

# Source files - organized by module
CORE_SOURCES = src/ir_builder.c \
              src/core/ir_component_types.c \
              src/core/ir_component_factory.c \
              src/core/ir_component_handler.c \
              src/core/ir_capability.c \
              src/core/ir_property_registry.c \
              src/core/ir_lifecycle_hooks.c

COMPONENT_SOURCES = src/components/button.c \
                   src/components/text.c \
                   src/components/flexbox.c \
                   src/components/input.c \
                   src/components/image.c \
                   src/components/canvas.c \
                   src/components/tabs.c \
                   src/components/table.c \
                   src/components/modal.c \
                   src/components/dropdown.c \
                   src/components/checkbox.c \
                   src/components/misc.c \
                   src/components/registry.c

STYLE_SOURCES = src/style/ir_style_builder.c \
                src/style/ir_style_types.c \
                src/style/ir_stylesheet.c \
                src/style/ir_style_vars.c \
                src/style/ir_gradient.c

LAYOUT_SOURCES = src/layout/ir_layout.c \
                 src/layout/ir_layout_builder.c \
                 src/layout/ir_layout_debug.c \
                 src/layout/ir_layout_strategy.c \
                 src/layout/strategies/strategy_container.c \
                 src/layout/strategies/strategy_row.c \
                 src/layout/strategies/strategy_column.c \
                 src/layout/strategies/strategy_text.c \
                 src/layout/strategies/strategy_image.c \
                 src/layout/strategies/strategy_canvas.c \
                 src/layout/strategies/strategy_foreach.c

LOGIC_SOURCES = src/logic/ir_logic.c \
                src/logic/ir_executor.c \
                src/logic/ir_expression.c \
                src/logic/ir_expression_parser.c \
                src/logic/ir_expression_compiler.c \
                src/logic/ir_expression_cache.c \
                src/logic/ir_builtin_registry.c \
                src/logic/ir_foreach.c \
                src/logic/ir_foreach_expand.c

EVENT_SOURCES = src/events/ir_event_builder.c \
                src/events/ir_event_types.c \
                src/events/ir_input.c \
                src/events/ir_hit_test.c

SERIALIZATION_SOURCES = src/serialization/ir_serialization.c \
                       src/serialization/ir_json_serialize.c \
                       src/serialization/ir_json_deserialize.c \
                       src/serialization/ir_json_context.c \
                       src/serialization/ir_serialize_types.c

STATE_SOURCES = src/state/ir_state_manager.c \
                src/state/ir_instance.c \
                src/state/ir_hot_reload.c \
                src/state/ir_reactive_manifest.c \
                src/state/ir_module_refs.c

FEATURE_SOURCES = src/features/ir_tabgroup.c \
                  src/features/ir_table.c \
                  src/features/ir_text_shaping.c \
                  src/features/ir_markdown.c \
                  src/features/ir_native_canvas.c
# Note: Animation system moved to kryon-plugins/animations

UTIL_SOURCES = src/utils/ir_string_builder.c \
               src/utils/ir_buffer.c \
               src/utils/ir_log.c \
               src/utils/ir_json_helpers.c \
               src/utils/ir_hashmap.c \
               src/utils/ir_memory.c \
               src/utils/ir_color_utils.c \
               src/utils/ir_validation.c \
               src/utils/ir_format_detect.c \
               src/utils/ir_asset.c \
               src/utils/ir_krb.c \
               src/utils/ir_lua_modules.c \
               src/utils/ir_source_structures.c \
               src/utils/ir_json.c

PARSER_SOURCES = parsers/kry/kry_allocator.c \
                 parsers/kry/kry_lexer.c \
                 parsers/kry/kry_ast_factory.c \
                 parsers/kry/kry_tokens.c \
                 parsers/kry/kry_to_ir_context.c \
                 parsers/kry/kry_to_ir_expressions.c \
                 parsers/kry/kry_to_ir_properties.c \
                 parsers/kry/kry_to_ir_functions.c \
                 parsers/kry/kry_parser.c \
                 parsers/kry/kry_to_ir.c \
                 parsers/kry/kry_expression.c \
                 parsers/kry/kry_arrow_registry.c \
                 parsers/kry/kry_struct_parser.c \
                 parsers/kry/kry_struct_to_ir.c \
                 parsers/kry/kry_ir_validator.c \
                 parsers/markdown/markdown_ast.c \
                 parsers/markdown/markdown_parser.c \
                 parsers/markdown/markdown_to_ir.c \
                 parsers/html/html_ast.c \
                 parsers/html/html_to_ir.c \
                 parsers/html/html_parser.c \
                 parsers/html/css_parser.c \
                 parsers/html/css_stylesheet.c \
                 parsers/c/c_parser.c \
                 parsers/tsx/tsx_parser.c \
                 parsers/lua/lua_parser.c \
                 parsers/lua/module_collector.c \
                 parsers/lua/require_tracker.c \
                 parsers/hare/hare_parser.c \
                 parsers/parser_utils.c

# All IR sources
IR_SOURCES = $(CORE_SOURCES) \
             $(COMPONENT_SOURCES) \
             $(STYLE_SOURCES) \
             $(LAYOUT_SOURCES) \
             $(LOGIC_SOURCES) \
             $(EVENT_SOURCES) \
             $(SERIALIZATION_SOURCES) \
             $(STATE_SOURCES) \
             $(FEATURE_SOURCES) \
             $(UTIL_SOURCES) \
             $(PARSER_SOURCES) \
             ../bindings/c/kryon.c \
             $(CJSON_DIR)/cJSON.c \
             $(TOML_DIR)/toml.c

# Output files
IR_STATIC_LIB = $(BUILD_DIR)/libkryon_ir.a
IR_SHARED_LIB = $(BUILD_DIR)/libkryon_ir.so

# Default target
all: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Static-only target (for minimal builds that don't need shared library)
static: $(IR_STATIC_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files with automatic dependency generation
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build static library
$(IR_STATIC_LIB): $(IR_SOURCES:.c=.o) | $(BUILD_DIR)
	$(AR) $(ARFLAGS) $@ $^

# Build shared library
$(IR_SHARED_LIB): $(IR_SOURCES:.c=.o) | $(BUILD_DIR)
	$(CC) -shared -o $@ $^ $(HARFBUZZ_LIBS) -lm

# Install library
install: all
	@echo "Installing Kryon IR library..."
	@mkdir -p $(BUILD_DIR)
	@cp $(IR_STATIC_LIB) $(BUILD_DIR)/
	@if [ -f $(IR_SHARED_LIB) ]; then \
		cp $(IR_SHARED_LIB) $(BUILD_DIR)/; \
	fi

# Clean build artifacts
clean:
	rm -f $(IR_SOURCES:.c=.o)
	rm -f $(IR_STATIC_LIB) $(IR_SHARED_LIB)
	rm -f src/*/*.o src/*/*/*.o parsers/*/*.o

# Clean all (including dependencies)
cleanall: clean
	rm -f $(CJSON_DIR)/*.o $(TOML_DIR)/*.o
	rm -rf $(BUILD_DIR)

# ASAN/UBSan build for debugging
asan-ubsan: CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -g
asan-ubsan: clean all

# Test target
test: all
	@echo "Running IR tests..."
	@$(MAKE) -C ../tests/unit/ir test

# Help target
help:
	@echo "Kryon IR Build System"
	@echo "====================="
	@echo ""
	@echo "Targets:"
	@echo "  all         - Build static and shared libraries (default)"
	@echo "  static      - Build static library only"
	@echo "  shared      - Build shared library only"
	@echo "  install     - Install libraries to build directory"
	@echo "  clean       - Remove build artifacts"
	@echo "  cleanall    - Remove all artifacts including dependencies"
	@echo "  asan-ubsan  - Build with AddressSanitizer and UBSan"
	@echo "  test        - Run IR tests"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Dependencies:"
	@echo "  HarfBuzz    - REQUIRED for text shaping"
	@echo "  FriBidi     - REQUIRED for bidirectional text"
	@echo "  FreeType    - REQUIRED for font rendering"
	@echo ""
	@echo "Install dependencies:"
	@echo "  Ubuntu/Debian: apt install libharfbuzz-dev libfribidi-dev libfreetype-dev"
	@echo "  Nix:         nix-shell"

.PHONY: all static shared install clean cleanall asan-ubsan test help
