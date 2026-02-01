# Makefile for Kryon IR System
# Compiles the core IR library that can be used by frontends and backends

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -Iinclude -I../include
AR = ar
ARFLAGS = rcs

# Single-pass layout system (ONLY system - no legacy code)
CFLAGS += -DIR_USE_SINGLE_PASS_LAYOUT

# System libraries via pkg-config (HarfBuzz, FreeType, FriBidi)
# These must be available via shell.nix or system package manager
HARFBUZZ_CFLAGS := $(shell pkg-config --cflags harfbuzz freetype2)
HARFBUZZ_LIBS := $(shell pkg-config --libs harfbuzz freetype2)
FRIBIDI_CFLAGS := $(shell pkg-config --cflags fribidi)
FRIBIDI_LIBS := $(shell pkg-config --libs fribidi)

# Add system library CFLAGS
CFLAGS += $(HARFBUZZ_CFLAGS) $(FRIBIDI_CFLAGS)

# Text shaping object file - requires system libraries
IR_TEXT_SHAPING_OBJ = src/features/text_shaping.o
IR_TEXT_SHAPING_LIBS = $(HARFBUZZ_LIBS) $(FRIBIDI_LIBS)

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
# FriBidi is now included via pkg-config
# Internal utils and bindings paths (for ir_constants.h, ir_c_metadata.h, etc.)
CFLAGS += -Isrc/utils -I../bindings/c

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
                 src/layout/layout_helpers.c \
                 src/layout/strategies/strategy_container.c \
                 src/layout/strategies/strategy_row.c \
                 src/layout/strategies/strategy_column.c \
                 src/layout/strategies/strategy_text.c \
                 src/layout/strategies/strategy_image.c \
                 src/layout/strategies/strategy_canvas.c \
                 src/layout/strategies/strategy_for.c

LOGIC_SOURCES = src/logic/ir_logic.c \
                src/logic/ir_executor.c \
                src/logic/ir_expression.c \
                src/logic/ir_expression_parser.c \
                src/logic/ir_expression_compiler.c \
                src/logic/ir_expression_cache.c \
                src/logic/ir_builtin_registry.c \
                src/logic/ir_for.c \
                src/logic/ir_for_expand.c

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
                  src/features/ir_markdown.c \
                  src/features/ir_native_canvas.c
# Note: Animation system moved to kryon-plugins/animations
# Note: Text shaping requires harfbuzz - not currently available

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
               src/utils/ir_source_structures.c \
               src/utils/ir_json.c \
               src/parser_interface.c

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
                 parsers/kry/kry_parser_adapter.c \
                 parsers/markdown/markdown_ast.c \
                 parsers/markdown/markdown_parser.c \
                 parsers/markdown/markdown_to_ir.c \
                 parsers/markdown/markdown_parser_adapter.c \
                 parsers/html/html_ast.c \
                 parsers/html/html_to_ir.c \
                 parsers/html/html_parser.c \
                 parsers/html/css_parser.c \
                 parsers/html/css_stylesheet.c \
                 parsers/html/html_parser_adapter.c \
                 parsers/c/c_parser.c \
                 parsers/c/c_parser_adapter.c \
                 parsers/tcl/tcl_lexer.c \
                 parsers/tcl/tcl_parser.c \
                 parsers/tcl/tcl_to_ir.c \
                 parsers/tcl/tcl_widget_registry.c \
                 parsers/tcl/tcl_parser_adapter.c \
                 parsers/limbo/limbo_lexer.c \
                 parsers/limbo/limbo_parser.c \
                 parsers/limbo/limbo_ast.c \
                 parsers/limbo/limbo_to_ir.c \
                 parsers/limbo/limbo_parser_adapter.c \
                 parsers/parser_utils.c \
                 parsers/common/parser_io_utils.c \
                 src/core/parser_registry.c

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

# System libraries (HarfBuzz, FreeType, FriBidi) are installed via package manager
# No need to build them from source
# On NixOS/nix-shell: already available via shell.nix
# On Ubuntu/Debian: sudo apt-get install libharfbuzz-dev libfreetype-dev libfribidi-dev

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
	@echo "Dependencies (all vendored in third_party/):"
	@echo "  HarfBuzz    - Text shaping (static)"
	@echo "  FreeType    - Font rendering (static)"
	@echo "  FriBidi     - Bidirectional text (static)"
	@echo "  cJSON       - JSON parsing (static)"
	@echo "  tomlc99     - TOML parsing (static)"
	@echo "  libtickit   - Terminal UI (static)"
	@echo ""
	@echo "Dynamic dependencies:"
	@echo "  SDL3/Raylib - Rendering backends (system)"
	@echo ""

.PHONY: all static shared install clean cleanall asan-ubsan test help
