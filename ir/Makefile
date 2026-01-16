# Makefile for Kryon IR System
# Compiles the core IR library that can be used by frontends and backends

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Single-pass layout system (ONLY system - no legacy code)
CFLAGS += -DIR_USE_SINGLE_PASS_LAYOUT

# HarfBuzz, FreeType, and FriBidi for text shaping
# These must be passed in from the environment or overridden on command line
HARFBUZZ_CFLAGS ?= $(shell pkg-config --cflags harfbuzz freetype2 fribidi 2>/dev/null || echo "")
HARFBUZZ_LIBS ?= $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || echo "")

# Check if harfbuzz is available
HAS_HARFBUZZ = $(shell pkg-config --exists harfbuzz && echo 1 || echo 0)

# Text shaping object file (only if harfbuzz is available)
# When harfbuzz is not available, use the stub implementation
ifeq ($(HAS_HARFBUZZ),1)
IR_TEXT_SHAPING_OBJ = ir_text_shaping.o
IR_TEXT_SHAPING_LIBS = $(HARFBUZZ_LIBS)
else
IR_TEXT_SHAPING_OBJ = ir_bidi_stub.o
IR_TEXT_SHAPING_LIBS =
endif

# Directories
IR_DIR = .
BUILD_DIR = ../build
INCLUDE_DIR = .

# Third-party directories
THIRD_PARTY_DIR = ../third_party
CJSON_DIR = $(THIRD_PARTY_DIR)/cJSON
STB_DIR = $(THIRD_PARTY_DIR)/stb
TOML_DIR = $(THIRD_PARTY_DIR)/tomlc99

# Third-party include paths
CFLAGS += -I$(CJSON_DIR) -I$(STB_DIR) -I$(TOML_DIR)

# Source files
IR_SOURCES = ir_log.c ir_string_builder.c ir_json_helpers.c ir_component_types.c ir_event_types.c ir_style_types.c ir_color_utils.c ir_component_factory.c ir_builder.c ir_instance.c ir_buffer.c ir_serialize_types.c ir_serialization.c ir_validation.c ir_style_vars.c ir_layout.c ir_layout_debug.c ir_memory.c ir_hashmap.c ir_animation.c ir_transition.c ir_text_shaping.c ir_hot_reload.c ir_format_detect.c ir_json.c ir_json_serialize.c ir_json_deserialize.c ir_json_context.c ir_reactive_manifest.c ir_source_structures.c ir_expression.c ir_expression_compiler.c ir_expression_cache.c ir_logic.c ir_executor.c ir_krb.c ir_stylesheet.c ir_foreach.c ir_foreach_expand.c ir_component_handler.c ir_state_manager.c ir_capability.c layout/ir_layout_strategy.c layout/strategy_container.c layout/strategy_text.c layout/strategy_row.c layout/strategy_column.c layout/strategy_image.c layout/strategy_canvas.c layout/strategy_foreach.c parsers/kry/kry_parser.c parsers/kry/kry_to_ir.c ir_input.c ir_asset.c ir_native_canvas.c parsers/markdown/markdown_ast.c parsers/markdown/markdown_parser.c parsers/markdown/markdown_to_ir.c parsers/html/html_ast.c parsers/html/html_parser.c parsers/html/html_to_ir.c parsers/html/css_parser.c parsers/html/css_stylesheet.c parsers/c/c_parser.c parsers/tsx/tsx_parser.c parsers/lua/lua_parser.c ../bindings/c/kryon.c components/ir_component_registry.c components/ir_component_text.c components/ir_component_flexbox.c components/ir_component_button.c components/ir_component_input.c components/ir_component_checkbox.c components/ir_component_dropdown.c components/ir_component_table.c components/ir_component_image.c components/ir_component_modal.c components/ir_component_canvas.c components/ir_component_misc.c components/ir_component_tabs.c $(CJSON_DIR)/cJSON.c $(TOML_DIR)/toml.c
IR_HEADERS = ir_constants.h ir_log.h ir_string_builder.h ir_json_helpers.h ir_component_types.h ir_layout_types.h ir_style_types.h ir_color_utils.h ir_component_factory.h ir_properties.h ir_event_types.h ir_core.h ir_builder.h ir_instance.h ir_buffer.h ir_serialize_types.h ir_serialization.h ir_validation.h ir_style_vars.h ir_layout_debug.h ir_memory.h ir_hashmap.h ir_animation.h ir_transition.h ir_text_shaping.h ir_hot_reload.h ir_format_detect.h ir_expression.h ir_expression_compiler.h ir_expression_cache.h ir_expression_parser.h ir_builtin_registry.h ir_logic.h ir_executor.h ir_krb.h ir_stylesheet.h ir_foreach.h ir_foreach_expand.h ir_component_handler.h ir_state_manager.h ir_capability.h layout/ir_layout_strategy.h parsers/kry/kry_parser.h ir_input.h ir_asset.h ir_native_canvas.h parsers/markdown/markdown_ast.h parsers/markdown/markdown_parser.h parsers/html/html_ast.h parsers/html/html_parser.h parsers/html/html_to_ir.h parsers/html/css_parser.h parsers/html/css_stylesheet.h parsers/c/c_parser.c parsers/tsx/tsx_parser.h parsers/lua/lua_parser.h ../bindings/c/kryon.h components/ir_component_registry.h components/ir_component_text.h components/ir_component_flexbox.h components/ir_component_button.h components/ir_component_input.h components/ir_component_checkbox.h components/ir_component_dropdown.h components/ir_component_table.h components/ir_component_image.h components/ir_component_modal.h components/ir_component_canvas.h components/ir_component_misc.h components/ir_component_tabs.h ir_tabgroup.h ir_style_builder.h ir_layout_builder.h ir_event_builder.h ir_module_refs.h ir_table.h ir_markdown.h ir_animation_builder.h ir_gradient.h ir_hit_test.h $(CJSON_DIR)/cJSON.h $(STB_DIR)/stb_image.h $(TOML_DIR)/toml.h

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

# Compile object files
ir_builder.o: ir_builder.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_builder.c -o ir_builder.o

ir_tabgroup.o: ir_tabgroup.c ir_tabgroup.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_tabgroup.c -o ir_tabgroup.o

ir_style_builder.o: ir_style_builder.c ir_style_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_style_builder.c -o ir_style_builder.o

ir_layout_builder.o: ir_layout_builder.c ir_layout_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_layout_builder.c -o ir_layout_builder.o

ir_event_builder.o: ir_event_builder.c ir_event_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_event_builder.c -o ir_event_builder.o

ir_module_refs.o: ir_module_refs.c ir_module_refs.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_module_refs.c -o ir_module_refs.o

ir_table.o: ir_table.c ir_table.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_table.c -o ir_table.o

ir_markdown.o: ir_markdown.c ir_markdown.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_markdown.c -o ir_markdown.o

ir_animation_builder.o: ir_animation_builder.c ir_animation_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_animation_builder.c -o ir_animation_builder.o

ir_gradient.o: ir_gradient.c ir_gradient.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_gradient.c -o ir_gradient.o

ir_hit_test.o: ir_hit_test.c ir_hit_test.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hit_test.c -o ir_hit_test.o

ir_instance.o: ir_instance.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_instance.c -o ir_instance.o

ir_buffer.o: ir_buffer.c ir_buffer.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_buffer.c -o ir_buffer.o

ir_serialize_types.o: ir_serialize_types.c ir_serialize_types.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_serialize_types.c -o ir_serialize_types.o

ir_serialization.o: ir_serialization.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_serialization.c -o ir_serialization.o

ir_validation.o: ir_validation.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_validation.c -o ir_validation.o

ir_style_vars.o: ir_style_vars.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_style_vars.c -o ir_style_vars.o

ir_layout.o: ir_layout.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_layout.c -o ir_layout.o

ir_layout_debug.o: ir_layout_debug.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_layout_debug.c -o ir_layout_debug.o

layout/ir_layout_strategy.o: layout/ir_layout_strategy.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/ir_layout_strategy.c -o layout/ir_layout_strategy.o

layout/strategy_container.o: layout/strategy_container.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_container.c -o layout/strategy_container.o

layout/strategy_text.o: layout/strategy_text.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_text.c -o layout/strategy_text.o

layout/strategy_row.o: layout/strategy_row.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_row.c -o layout/strategy_row.o

layout/strategy_column.o: layout/strategy_column.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_column.c -o layout/strategy_column.o

layout/strategy_image.o: layout/strategy_image.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_image.c -o layout/strategy_image.o

layout/strategy_canvas.o: layout/strategy_canvas.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_canvas.c -o layout/strategy_canvas.o

layout/strategy_foreach.o: layout/strategy_foreach.c layout/ir_layout_strategy.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c layout/strategy_foreach.c -o layout/strategy_foreach.o

ir_memory.o: ir_memory.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_memory.c -o ir_memory.o

ir_hashmap.o: ir_hashmap.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hashmap.c -o ir_hashmap.o

ir_animation.o: ir_animation.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_animation.c -o ir_animation.o -lm

ir_transition.o: ir_transition.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_transition.c -o ir_transition.o -lm

ir_text_shaping.o: ir_text_shaping.c $(IR_HEADERS)
ifeq ($(HAS_HARFBUZZ),1)
	$(CC) $(CFLAGS) $(HARFBUZZ_CFLAGS) -I$(INCLUDE_DIR) -c ir_text_shaping.c -o ir_text_shaping.o
else
	@echo "Skipping ir_text_shaping.c (harfbuzz not available, using stub)"
endif

ir_bidi_stub.o: ir_bidi_stub.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_bidi_stub.c -o ir_bidi_stub.o

ir_hot_reload.o: ir_hot_reload.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hot_reload.c -o ir_hot_reload.o

ir_format_detect.o: ir_format_detect.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_format_detect.c -o ir_format_detect.o

ir_log.o: ir_log.c ir_log.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_log.c -o ir_log.o

ir_string_builder.o: ir_string_builder.c ir_string_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_string_builder.c -o ir_string_builder.o

ir_json_helpers.o: ir_json_helpers.c ir_json_helpers.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_helpers.c -o ir_json_helpers.o

ir_component_types.o: ir_component_types.c ir_component_types.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_component_types.c -o ir_component_types.o

ir_event_types.o: ir_event_types.c ir_event_types.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_event_types.c -o ir_event_types.o

ir_style_types.o: ir_style_types.c ir_style_types.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_style_types.c -o ir_style_types.o

ir_color_utils.o: ir_color_utils.c ir_color_utils.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_color_utils.c -o ir_color_utils.o

ir_component_factory.o: ir_component_factory.c ir_component_factory.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_component_factory.c -o ir_component_factory.o

$(CJSON_DIR)/cJSON.o: $(CJSON_DIR)/cJSON.c $(CJSON_DIR)/cJSON.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $(CJSON_DIR)/cJSON.c -o $(CJSON_DIR)/cJSON.o

$(TOML_DIR)/toml.o: $(TOML_DIR)/toml.c $(TOML_DIR)/toml.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $(TOML_DIR)/toml.c -o $(TOML_DIR)/toml.o

ir_json.o: ir_json.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json.c -o ir_json.o

ir_json_serialize.o: ir_json_serialize.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_serialize.c -o ir_json_serialize.o

ir_json_deserialize.o: ir_json_deserialize.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_deserialize.c -o ir_json_deserialize.o

ir_json_context.o: ir_json_context.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_context.c -o ir_json_context.o

ir_reactive_manifest.o: ir_reactive_manifest.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_reactive_manifest.c -o ir_reactive_manifest.o

ir_source_structures.o: ir_source_structures.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_source_structures.c -o ir_source_structures.o

ir_expression.o: ir_expression.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_expression.c -o ir_expression.o

ir_expression_compiler.o: ir_expression_compiler.c ir_expression_compiler.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_expression_compiler.c -o ir_expression_compiler.o

ir_expression_parser.o: ir_expression_parser.c ir_expression_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_expression_parser.c -o ir_expression_parser.o

ir_expression_cache.o: ir_expression_cache.c ir_expression_cache.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_expression_cache.c -o ir_expression_cache.o

ir_builtin_registry.o: ir_builtin_registry.c ir_builtin_registry.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_builtin_registry.c -o ir_builtin_registry.o

ir_logic.o: ir_logic.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_logic.c -o ir_logic.o

ir_executor.o: ir_executor.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_executor.c -o ir_executor.o

ir_input.o: ir_input.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_input.c -o ir_input.o

ir_asset.o: ir_asset.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_asset.c -o ir_asset.o

ir_capability.o: ir_capability.c ir_capability.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_capability.c -o ir_capability.o

ir_native_canvas.o: ir_native_canvas.c ir_native_canvas.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_native_canvas.c -o ir_native_canvas.o

parsers/markdown/markdown_ast.o: parsers/markdown/markdown_ast.c parsers/markdown/markdown_ast.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/markdown/markdown_ast.c -o parsers/markdown/markdown_ast.o

parsers/markdown/markdown_parser.o: parsers/markdown/markdown_parser.c parsers/markdown/markdown_parser.h parsers/markdown/markdown_ast.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/markdown/markdown_parser.c -o parsers/markdown/markdown_parser.o

parsers/markdown/markdown_to_ir.o: parsers/markdown/markdown_to_ir.c parsers/markdown/markdown_ast.h ir_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/markdown/markdown_to_ir.c -o parsers/markdown/markdown_to_ir.o

parsers/html/html_ast.o: parsers/html/html_ast.c parsers/html/html_ast.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/html/html_ast.c -o parsers/html/html_ast.o

parsers/html/html_parser.o: parsers/html/html_parser.c parsers/html/html_parser.h parsers/html/html_ast.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/html/html_parser.c -o parsers/html/html_parser.o

parsers/html/html_to_ir.o: parsers/html/html_to_ir.c parsers/html/html_to_ir.h parsers/html/html_ast.h ir_builder.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/html/html_to_ir.c -o parsers/html/html_to_ir.o

parsers/html/css_parser.o: parsers/html/css_parser.c parsers/html/css_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/html/css_parser.c -o parsers/html/css_parser.o

parsers/html/css_stylesheet.o: parsers/html/css_stylesheet.c parsers/html/css_stylesheet.h parsers/html/css_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/html/css_stylesheet.c -o parsers/html/css_stylesheet.o

ir_krb.o: ir_krb.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_krb.c -o ir_krb.o

ir_stylesheet.o: ir_stylesheet.c ir_stylesheet.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_stylesheet.c -o ir_stylesheet.o

ir_foreach.o: ir_foreach.c ir_foreach.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_foreach.c -o ir_foreach.o

ir_foreach_expand.o: ir_foreach_expand.c ir_foreach_expand.h ir_foreach.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_foreach_expand.c -o ir_foreach_expand.o

ir_component_handler.o: ir_component_handler.c ir_component_handler.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_component_handler.c -o ir_component_handler.o

ir_state_manager.o: ir_state_manager.c ir_state_manager.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_state_manager.c -o ir_state_manager.o

parsers/kry/kry_parser.o: parsers/kry/kry_parser.c parsers/kry/kry_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/kry/kry_parser.c -o parsers/kry/kry_parser.o

parsers/kry/kry_to_ir.o: parsers/kry/kry_to_ir.c parsers/kry/kry_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/kry/kry_to_ir.c -o parsers/kry/kry_to_ir.o

components/ir_component_registry.o: components/ir_component_registry.c components/ir_component_registry.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_registry.c -o components/ir_component_registry.o

components/ir_component_text.o: components/ir_component_text.c components/ir_component_text.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_text.c -o components/ir_component_text.o

components/ir_component_flexbox.o: components/ir_component_flexbox.c components/ir_component_flexbox.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_flexbox.c -o components/ir_component_flexbox.o

components/ir_component_button.o: components/ir_component_button.c components/ir_component_button.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_button.c -o components/ir_component_button.o

components/ir_component_input.o: components/ir_component_input.c components/ir_component_input.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_input.c -o components/ir_component_input.o

components/ir_component_checkbox.o: components/ir_component_checkbox.c components/ir_component_checkbox.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_checkbox.c -o components/ir_component_checkbox.o

components/ir_component_dropdown.o: components/ir_component_dropdown.c components/ir_component_dropdown.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_dropdown.c -o components/ir_component_dropdown.o

components/ir_component_table.o: components/ir_component_table.c components/ir_component_table.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_table.c -o components/ir_component_table.o

components/ir_component_image.o: components/ir_component_image.c components/ir_component_image.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_image.c -o components/ir_component_image.o

components/ir_component_modal.o: components/ir_component_modal.c components/ir_component_modal.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_modal.c -o components/ir_component_modal.o

components/ir_component_canvas.o: components/ir_component_canvas.c components/ir_component_canvas.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_canvas.c -o components/ir_component_canvas.o

components/ir_component_misc.o: components/ir_component_misc.c components/ir_component_misc.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_misc.c -o components/ir_component_misc.o

components/ir_component_tabs.o: components/ir_component_tabs.c components/ir_component_tabs.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c components/ir_component_tabs.c -o components/ir_component_tabs.o

parsers/c/c_parser.o: parsers/c/c_parser.c parsers/c/c_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/c/c_parser.c -o parsers/c/c_parser.o

parsers/tsx/tsx_parser.o: parsers/tsx/tsx_parser.c parsers/tsx/tsx_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/tsx/tsx_parser.c -o parsers/tsx/tsx_parser.o

parsers/lua/lua_parser.o: parsers/lua/lua_parser.c parsers/lua/lua_parser.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/lua/lua_parser.c -o parsers/lua/lua_parser.o

parsers/lua/require_tracker.o: parsers/lua/require_tracker.c parsers/lua/require_tracker.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/lua/require_tracker.c -o parsers/lua/require_tracker.o

parsers/lua/module_collector.o: parsers/lua/module_collector.c parsers/lua/module_collector.h parsers/lua/require_tracker.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c parsers/lua/module_collector.c -o parsers/lua/module_collector.o

../bindings/c/kryon.o: ../bindings/c/kryon.c ../bindings/c/kryon.h $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -I../bindings/c -c ../bindings/c/kryon.c -o ../bindings/c/kryon.o

# Create static library
$(IR_STATIC_LIB): $(BUILD_DIR) ir_log.o ir_string_builder.o ir_json_helpers.o ir_component_types.o ir_event_types.o ir_style_types.o ir_color_utils.o ir_component_factory.o ir_builder.o ir_tabgroup.o ir_style_builder.o ir_layout_builder.o ir_event_builder.o ir_module_refs.o ir_table.o ir_markdown.o ir_animation_builder.o ir_gradient.o ir_hit_test.o ir_instance.o ir_buffer.o ir_serialize_types.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_layout_debug.o ir_memory.o ir_hashmap.o ir_animation.o ir_transition.o $(IR_TEXT_SHAPING_OBJ) ir_hot_reload.o ir_format_detect.o ir_json.o ir_json_serialize.o ir_json_deserialize.o ir_json_context.o ir_reactive_manifest.o ir_source_structures.o ir_expression.o ir_expression_compiler.o ir_expression_cache.o ir_builtin_registry.o ir_expression_parser.o ir_logic.o ir_executor.o ir_krb.o ir_stylesheet.o ir_foreach.o ir_foreach_expand.o ir_component_handler.o ir_state_manager.o ir_capability.o layout/ir_layout_strategy.o layout/strategy_container.o layout/strategy_text.o layout/strategy_row.o layout/strategy_column.o layout/strategy_image.o layout/strategy_canvas.o layout/strategy_foreach.o parsers/kry/kry_parser.o parsers/kry/kry_to_ir.o ir_input.o ir_asset.o ir_native_canvas.o parsers/markdown/markdown_ast.o parsers/markdown/markdown_parser.o parsers/markdown/markdown_to_ir.o parsers/html/html_ast.o parsers/html/html_parser.o parsers/html/html_to_ir.o parsers/html/css_parser.o parsers/html/css_stylesheet.o parsers/c/c_parser.o parsers/tsx/tsx_parser.o parsers/lua/lua_parser.o parsers/lua/require_tracker.o parsers/lua/module_collector.o ../bindings/c/kryon.o components/ir_component_registry.o components/ir_component_text.o components/ir_component_flexbox.o components/ir_component_button.o components/ir_component_input.o components/ir_component_checkbox.o components/ir_component_dropdown.o components/ir_component_table.o components/ir_component_image.o components/ir_component_modal.o components/ir_component_canvas.o components/ir_component_misc.o components/ir_component_tabs.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o
	$(AR) $(ARFLAGS) $@ ir_log.o ir_string_builder.o ir_json_helpers.o ir_component_types.o ir_event_types.o ir_style_types.o ir_color_utils.o ir_component_factory.o ir_builder.o ir_tabgroup.o ir_style_builder.o ir_layout_builder.o ir_event_builder.o ir_module_refs.o ir_table.o ir_markdown.o ir_animation_builder.o ir_gradient.o ir_hit_test.o ir_instance.o ir_buffer.o ir_serialize_types.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_layout_debug.o ir_memory.o ir_hashmap.o ir_animation.o ir_transition.o $(IR_TEXT_SHAPING_OBJ) ir_hot_reload.o ir_format_detect.o ir_json.o ir_json_serialize.o ir_json_deserialize.o ir_json_context.o ir_reactive_manifest.o ir_source_structures.o ir_expression.o ir_expression_compiler.o ir_expression_cache.o ir_builtin_registry.o ir_expression_parser.o ir_logic.o ir_executor.o ir_krb.o ir_stylesheet.o ir_foreach.o ir_foreach_expand.o ir_component_handler.o ir_state_manager.o ir_capability.o layout/ir_layout_strategy.o layout/strategy_container.o layout/strategy_text.o layout/strategy_row.o layout/strategy_column.o layout/strategy_image.o layout/strategy_canvas.o layout/strategy_foreach.o parsers/kry/kry_parser.o parsers/kry/kry_to_ir.o ir_input.o ir_asset.o ir_native_canvas.o parsers/markdown/markdown_ast.o parsers/markdown/markdown_parser.o parsers/markdown/markdown_to_ir.o parsers/html/html_ast.o parsers/html/html_parser.o parsers/html/html_to_ir.o parsers/html/css_parser.o parsers/html/css_stylesheet.o parsers/c/c_parser.o parsers/tsx/tsx_parser.o parsers/lua/lua_parser.o parsers/lua/require_tracker.o parsers/lua/module_collector.o ../bindings/c/kryon.o components/ir_component_registry.o components/ir_component_text.o components/ir_component_flexbox.o components/ir_component_button.o components/ir_component_input.o components/ir_component_checkbox.o components/ir_component_dropdown.o components/ir_component_table.o components/ir_component_image.o components/ir_component_modal.o components/ir_component_canvas.o components/ir_component_misc.o components/ir_component_tabs.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o

# Create shared library
$(IR_SHARED_LIB): $(BUILD_DIR) ir_log.o ir_string_builder.o ir_json_helpers.o ir_component_types.o ir_event_types.o ir_style_types.o ir_color_utils.o ir_component_factory.o ir_builder.o ir_tabgroup.o ir_style_builder.o ir_layout_builder.o ir_event_builder.o ir_module_refs.o ir_table.o ir_markdown.o ir_animation_builder.o ir_gradient.o ir_hit_test.o ir_instance.o ir_buffer.o ir_serialize_types.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_layout_debug.o ir_memory.o ir_hashmap.o ir_animation.o ir_transition.o $(IR_TEXT_SHAPING_OBJ) ir_hot_reload.o ir_format_detect.o ir_json.o ir_json_serialize.o ir_json_deserialize.o ir_json_context.o ir_reactive_manifest.o ir_source_structures.o ir_expression.o ir_expression_compiler.o ir_expression_cache.o ir_builtin_registry.o ir_expression_parser.o ir_logic.o ir_executor.o ir_krb.o ir_stylesheet.o ir_foreach.o ir_foreach_expand.o ir_component_handler.o ir_state_manager.o ir_capability.o layout/ir_layout_strategy.o layout/strategy_container.o layout/strategy_text.o layout/strategy_row.o layout/strategy_column.o layout/strategy_image.o layout/strategy_canvas.o layout/strategy_foreach.o parsers/kry/kry_parser.o parsers/kry/kry_to_ir.o ir_input.o ir_asset.o ir_native_canvas.o parsers/markdown/markdown_ast.o parsers/markdown/markdown_parser.o parsers/markdown/markdown_to_ir.o parsers/html/html_ast.o parsers/html/html_parser.o parsers/html/html_to_ir.o parsers/html/css_parser.o parsers/html/css_stylesheet.o parsers/c/c_parser.o parsers/tsx/tsx_parser.o parsers/lua/lua_parser.o parsers/lua/require_tracker.o parsers/lua/module_collector.o ../bindings/c/kryon.o components/ir_component_registry.o components/ir_component_text.o components/ir_component_flexbox.o components/ir_component_button.o components/ir_component_input.o components/ir_component_checkbox.o components/ir_component_dropdown.o components/ir_component_table.o components/ir_component_image.o components/ir_component_modal.o components/ir_component_canvas.o components/ir_component_misc.o components/ir_component_tabs.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o
	$(CC) $(CFLAGS) -shared -o $@ ir_log.o ir_string_builder.o ir_json_helpers.o ir_component_types.o ir_event_types.o ir_style_types.o ir_color_utils.o ir_component_factory.o ir_builder.o ir_tabgroup.o ir_style_builder.o ir_layout_builder.o ir_event_builder.o ir_module_refs.o ir_table.o ir_markdown.o ir_animation_builder.o ir_gradient.o ir_hit_test.o ir_instance.o ir_buffer.o ir_serialize_types.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_layout_debug.o ir_memory.o ir_hashmap.o ir_animation.o ir_transition.o $(IR_TEXT_SHAPING_OBJ) ir_hot_reload.o ir_format_detect.o ir_json.o ir_json_serialize.o ir_json_deserialize.o ir_json_context.o ir_reactive_manifest.o ir_source_structures.o ir_expression.o ir_expression_compiler.o ir_expression_cache.o ir_builtin_registry.o ir_expression_parser.o ir_logic.o ir_executor.o ir_krb.o ir_stylesheet.o ir_foreach.o ir_foreach_expand.o ir_component_handler.o ir_state_manager.o ir_capability.o layout/ir_layout_strategy.o layout/strategy_container.o layout/strategy_text.o layout/strategy_row.o layout/strategy_column.o layout/strategy_image.o layout/strategy_canvas.o layout/strategy_foreach.o parsers/kry/kry_parser.o parsers/kry/kry_to_ir.o ir_input.o ir_asset.o ir_native_canvas.o parsers/markdown/markdown_ast.o parsers/markdown/markdown_parser.o parsers/markdown/markdown_to_ir.o parsers/html/html_ast.o parsers/html/html_parser.o parsers/html/html_to_ir.o parsers/html/css_parser.o parsers/html/css_stylesheet.o parsers/c/c_parser.o parsers/tsx/tsx_parser.o parsers/lua/lua_parser.o parsers/lua/require_tracker.o parsers/lua/module_collector.o ../bindings/c/kryon.o components/ir_component_registry.o components/ir_component_text.o components/ir_component_flexbox.o components/ir_component_button.o components/ir_component_input.o components/ir_component_checkbox.o components/ir_component_dropdown.o components/ir_component_table.o components/ir_component_image.o components/ir_component_modal.o components/ir_component_canvas.o components/ir_component_misc.o components/ir_component_tabs.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o -lm $(IR_TEXT_SHAPING_LIBS)

# Install headers
install-headers: $(BUILD_DIR)
	cp $(IR_HEADERS) $(BUILD_DIR)/

# Clean build artifacts
clean:
	rm -f *.o
	rm -f layout/*.o
	rm -f components/*.o
	rm -f parsers/kry/*.o parsers/markdown/*.o parsers/html/*.o parsers/c/*.o parsers/tsx/*.o parsers/lua/*.o
	rm -f ../bindings/c/*.o
	rm -f $(CJSON_DIR)/*.o $(TOML_DIR)/*.o
	rm -f $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Build and run organized tests (Phase 1 + Phase 2)
test: test-ir-organized

# Build and run VM test
test-vm: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_vm.c ir_vm.o -o $(BUILD_DIR)/test_vm
	$(BUILD_DIR)/test_vm

# Build and run VM integration test (Phase 1 spike)
test-vm-integration: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_vm_integration.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_vm_integration
	$(BUILD_DIR)/test_vm_integration

# Build and run event metadata test (Phase 2)
test-event-metadata: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_event_metadata.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_event_metadata
	$(BUILD_DIR)/test_event_metadata

# Build and run backend integration test (Phase 3)
test-phase3: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_phase3_backend_integration.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_phase3_backend_integration
	$(BUILD_DIR)/test_phase3_backend_integration

# Build and run multi-instance test (Phase 5-7)
test-multi-instance: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) tests/test_multi_instance.c $(IR_STATIC_LIB) -lm $(IR_TEXT_SHAPING_LIBS) -o $(BUILD_DIR)/test_multi_instance
	$(BUILD_DIR)/test_multi_instance

# Build and run expression phase 1 test
test-expression-phase1: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_expression_phase1.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_expression_phase1
	$(BUILD_DIR)/test_expression_phase1

# Build and run expression parser test
test-expression-parser: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_expression_parser.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_expression_parser
	$(BUILD_DIR)/test_expression_parser

# Build and run expression compiler test
test-expression-compiler: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_expression_compiler.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_expression_compiler
	$(BUILD_DIR)/test_expression_compiler

# Run all expression tests
test-expression-all: test-expression-phase1 test-expression-parser test-expression-compiler test-expression-eval test-expression-cache test-expression-builtin test-integration

# Build and run expression evaluation test
test-expression-eval: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_expression_eval.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_expression_eval
	$(BUILD_DIR)/test_expression_eval

test-expression-cache: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_expression_cache.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_expression_cache
	$(BUILD_DIR)/test_expression_cache

test-expression-builtin: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_builtin_functions.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_builtin_functions
	$(BUILD_DIR)/test_builtin_functions

# Build and run integration test (Phase 7)
test-integration: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test_integration.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_integration
	$(BUILD_DIR)/test_integration

# ============================================================================
# Phase 1: Core IR Foundation Tests
# These test fundamental utilities: buffer, memory, string builder, json, color
# ============================================================================

# Buffer tests (IRBuffer - dynamic buffer for binary serialization)
test-ir-buffer: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_buffer.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_buffer
	$(BUILD_DIR)/test_ir_buffer

# Memory tests (pool and arena allocators)
test-ir-memory: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_memory.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_memory
	$(BUILD_DIR)/test_ir_memory

# String builder tests (IRStringBuilder - dynamic string concatenation)
test-ir-string-builder: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_string_builder.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_string_builder
	$(BUILD_DIR)/test_ir_string_builder

# JSON helpers tests (cJSON wrapper functions)
test-ir-json-helpers: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_json_helpers.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_json_helpers
	$(BUILD_DIR)/test_ir_json_helpers

# Color utils tests (color creation and named colors)
test-ir-color-utils: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_color_utils.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_color_utils
	$(BUILD_DIR)/test_ir_color_utils

# Run all Phase 1 core foundation tests
test-ir-core: test-ir-buffer test-ir-memory test-ir-string-builder test-ir-json-helpers test-ir-color-utils

# ============================================================================
# Phase 2: IR Core Types Tests
# These test core type systems: component types, layout, style, events, properties
# ============================================================================

# Component type tests (IRComponentType enum and conversion functions)
test-ir-component-types: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_component_types.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_component_types
	$(BUILD_DIR)/test_ir_component_types

# Layout type tests (IRDimension, IRAlignment, layout structures)
test-ir-layout-types: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_layout_types.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_layout_types
	$(BUILD_DIR)/test_ir_layout_types

# Style type tests (IRColor, IRGradient, style structures)
test-ir-style-types: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_style_types.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_style_types
	$(BUILD_DIR)/test_ir_style_types

# Event type tests (IREventType enum and conversion functions)
test-ir-event-types: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_event_types.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_event_types
	$(BUILD_DIR)/test_ir_event_types

# Property tests (IRFlexbox, IRGrid, IRSpacing, IRBorder, IRTypography, etc.)
test-ir-properties: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) test/test_ir_properties.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_properties
	$(BUILD_DIR)/test_ir_properties

# Run all Phase 2 core types tests
test-ir-types: test-ir-component-types test-ir-layout-types test-ir-style-types test-ir-event-types test-ir-properties

# State manager tests
test-ir-state-manager: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) tests/test_ir_state_manager.c $(IR_STATIC_LIB) -lm -o $(BUILD_DIR)/test_ir_state_manager
	$(BUILD_DIR)/test_ir_state_manager

# Run all organized IR tests (Phase 1 + Phase 2)
test-ir-organized: test-ir-core test-ir-types

# Run ALL tests (organized tests + legacy tests)
test-all: test-ir-organized test-expression-all test-integration

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Debug build with memory tracing (max verbosity)
debug-mem: CFLAGS += -g -DDEBUG -DIR_DEBUG_LEVEL=4
debug-mem: clean all

# Sanitizer flag definitions (preserve pkg-config CFLAGS)
ASAN_FLAGS = -g -fsanitize=address -fno-omit-frame-pointer -DDEBUG
UBSAN_FLAGS = -g -fsanitize=undefined -fno-omit-frame-pointer -DDEBUG
TSAN_FLAGS = -g -fsanitize=thread -fno-omit-frame-pointer -DDEBUG

# Sanitizer builds for catching bugs during testing
asan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(ASAN_FLAGS)"

ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(UBSAN_FLAGS)"

asan-ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(ASAN_FLAGS) $(UBSAN_FLAGS)"

tsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="$(CFLAGS) $(TSAN_FLAGS)"

# Help
help:
	@echo "Kryon IR System Build Targets:"
	@echo "  all                - Build static and shared libraries"
	@echo "  static             - Build static library only"
	@echo "  shared             - Build shared library only"
	@echo "  test               - Run organized tests (Phase 1 + Phase 2)"
	@echo "  test-all           - Run ALL tests (Phase 1 + Phase 2 + expressions + integration)"
	@echo "  test-ir-organized  - Run all Phase 1 + Phase 2 organized tests"
	@echo "  test-ir-core       - Run Phase 1 Core IR Foundation tests (buffer, memory, etc)"
	@echo "  test-ir-types      - Run Phase 2 IR Core Types tests"
	@echo "  test-ir-buffer     - Test IRBuffer (dynamic buffer for serialization)"
	@echo "  test-ir-memory     - Test pool/arena allocators"
	@echo "  test-ir-string-builder - Test IRStringBuilder (string concatenation)"
	@echo "  test-ir-json-helpers - Test cJSON wrapper functions"
	@echo "  test-ir-color-utils - Test color utilities"
	@echo "  test-ir-component-types - Test component type system"
	@echo "  test-ir-layout-types - Test layout type system"
	@echo "  test-ir-style-types - Test style type system"
	@echo "  test-ir-event-types - Test event type system"
	@echo "  test-ir-properties - Test property types"
	@echo "  test-vm            - Build and run VM prototype test"
	@echo "  test-vm-integration - Build and run VM integration test (Phase 1)"
	@echo "  test-event-metadata - Build and run event metadata test (Phase 2)"
	@echo "  test-phase3        - Build and run Phase 3 backend test"
	@echo "  test-multi-instance - Build and run multi-instance test"
	@echo "  test-expression-all - Run all expression tests"
	@echo "  test-integration   - Build and run integration test (Phase 7)"
	@echo "  clean              - Remove build artifacts"
	@echo "  install-headers    - Copy headers to build directory"
	@echo "  debug              - Build with debug symbols"
	@echo "  debug-mem          - Build with debug symbols and memory tracing"
	@echo "  asan               - Build with AddressSanitizer"
	@echo "  ubsan              - Build with UndefinedBehaviorSanitizer"
	@echo "  asan-ubsan         - Build with both ASan and UBSan (recommended)"
	@echo "  tsan               - Build with ThreadSanitizer"
	@echo "  help               - Show this help"

.PHONY: all static clean test test-vm test-vm-integration test-event-metadata test-phase3 test-multi-instance test-expression-phase1 test-expression-parser test-expression-compiler test-expression-eval test-expression-cache test-expression-all test-integration test-ir-core test-ir-buffer test-ir-memory test-ir-string-builder test-ir-json-helpers test-ir-color-utils test-ir-component-types test-ir-layout-types test-ir-style-types test-ir-event-types test-ir-properties test-ir-types test-ir-organized test-all debug install-headers help asan ubsan asan-ubsan tsan