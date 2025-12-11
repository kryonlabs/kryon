# Makefile for Kryon IR System
# Compiles the core IR library that can be used by frontends and backends

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# HarfBuzz, FreeType, and FriBidi for text shaping
# Try pkg-config first, fallback to Nix store paths
HARFBUZZ_CFLAGS = $(shell pkg-config --cflags harfbuzz freetype2 fribidi 2>/dev/null || \
	echo "-I/nix/store/fyf45yywbqkfzli3k153lngkkdhk7w58-harfbuzz-12.1.0-dev/include/harfbuzz \
	-I/nix/store/79b83lm0fwilxc816z4pcvd58j1j3cdd-freetype-2.13.3-dev/include/freetype2 \
	-I/nix/store/79b83lm0fwilxc816z4pcvd58j1j3cdd-freetype-2.13.3-dev/include \
	-I/nix/store/h329498yf07j5l2j7p3hf2bichgn24fq-fribidi-1.0.16-dev/include/fribidi")
HARFBUZZ_LIBS = $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || \
	echo "-L/nix/store/8mk1lafnnrx0a6lvjh69afm8w3i4pvqx-harfbuzz-12.1.0/lib \
	-L/nix/store/p1cqrricl0ch7qab8b8w3krhc91d890v-freetype-2.13.3/lib \
	-L/nix/store/62h5sfxr4s1qc3fghcan06viq4gms6yg-fribidi-1.0.16/lib \
	-lharfbuzz -lfreetype -lfribidi")

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
IR_SOURCES = ir_builder.c ir_serialization.c ir_validation.c ir_style_vars.c ir_layout.c ir_memory.c ir_hashmap.c ir_animation.c ir_text_shaping.c ir_hot_reload.c ir_format_detect.c ir_json_v2.c ir_reactive_manifest.c ir_expression.c ir_logic.c ir_executor.c ir_game_loop.c ir_input.c ir_sprite.c ir_particle.c ir_asset.c ir_audio.c ir_physics.c ir_entity.c ir_plugin.c $(CJSON_DIR)/cJSON.c $(TOML_DIR)/toml.c
IR_HEADERS = ir_core.h ir_builder.h ir_serialization.h ir_validation.h ir_style_vars.h ir_memory.h ir_hashmap.h ir_animation.h ir_text_shaping.h ir_hot_reload.h ir_format_detect.h ir_expression.h ir_logic.h ir_executor.h ir_game_loop.h ir_input.h ir_sprite.h ir_particle.h ir_asset.h ir_audio.h ir_physics.h ir_entity.h ir_plugin.h $(CJSON_DIR)/cJSON.h $(STB_DIR)/stb_image.h $(TOML_DIR)/toml.h

# Output files
IR_STATIC_LIB = $(BUILD_DIR)/libkryon_ir.a
IR_SHARED_LIB = $(BUILD_DIR)/libkryon_ir.so

# Default target
all: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files
ir_builder.o: ir_builder.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_builder.c -o ir_builder.o

ir_serialization.o: ir_serialization.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_serialization.c -o ir_serialization.o

ir_validation.o: ir_validation.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_validation.c -o ir_validation.o

ir_style_vars.o: ir_style_vars.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_style_vars.c -o ir_style_vars.o

ir_layout.o: ir_layout.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_layout.c -o ir_layout.o

ir_memory.o: ir_memory.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_memory.c -o ir_memory.o

ir_hashmap.o: ir_hashmap.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hashmap.c -o ir_hashmap.o

ir_animation.o: ir_animation.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_animation.c -o ir_animation.o -lm

ir_text_shaping.o: ir_text_shaping.c $(IR_HEADERS)
	$(CC) $(CFLAGS) $(HARFBUZZ_CFLAGS) -I$(INCLUDE_DIR) -c ir_text_shaping.c -o ir_text_shaping.o

ir_hot_reload.o: ir_hot_reload.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_hot_reload.c -o ir_hot_reload.o

ir_format_detect.o: ir_format_detect.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_format_detect.c -o ir_format_detect.o

$(CJSON_DIR)/cJSON.o: $(CJSON_DIR)/cJSON.c $(CJSON_DIR)/cJSON.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c $(CJSON_DIR)/cJSON.c -o $(CJSON_DIR)/cJSON.o

ir_json_v2.o: ir_json_v2.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_v2.c -o ir_json_v2.o

ir_reactive_manifest.o: ir_reactive_manifest.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_reactive_manifest.c -o ir_reactive_manifest.o

ir_expression.o: ir_expression.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_expression.c -o ir_expression.o

ir_logic.o: ir_logic.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_logic.c -o ir_logic.o

ir_executor.o: ir_executor.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_executor.c -o ir_executor.o

ir_game_loop.o: ir_game_loop.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_game_loop.c -o ir_game_loop.o

ir_input.o: ir_input.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_input.c -o ir_input.o

ir_sprite.o: ir_sprite.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_sprite.c -o ir_sprite.o

ir_particle.o: ir_particle.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_particle.c -o ir_particle.o

ir_asset.o: ir_asset.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_asset.c -o ir_asset.o

ir_audio.o: ir_audio.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_audio.c -o ir_audio.o

ir_physics.o: ir_physics.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_physics.c -o ir_physics.o

ir_entity.o: ir_entity.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_entity.c -o ir_entity.o

ir_plugin.o: ir_plugin.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_plugin.c -o ir_plugin.o

# Create static library
$(IR_STATIC_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o ir_reactive_manifest.o ir_expression.o ir_logic.o ir_executor.o ir_game_loop.o ir_input.o ir_sprite.o ir_particle.o ir_asset.o ir_audio.o ir_physics.o ir_entity.o ir_plugin.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o
	$(AR) $(ARFLAGS) $@ ir_builder.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o ir_reactive_manifest.o ir_expression.o ir_logic.o ir_executor.o ir_game_loop.o ir_input.o ir_sprite.o ir_particle.o ir_asset.o ir_audio.o ir_physics.o ir_entity.o ir_plugin.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o

# Create shared library
$(IR_SHARED_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o ir_reactive_manifest.o ir_expression.o ir_logic.o ir_executor.o ir_game_loop.o ir_input.o ir_sprite.o ir_particle.o ir_asset.o ir_audio.o ir_physics.o ir_entity.o ir_plugin.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o
	$(CC) $(CFLAGS) -shared -o $@ ir_builder.o ir_serialization.o ir_validation.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o ir_reactive_manifest.o ir_expression.o ir_logic.o ir_executor.o ir_game_loop.o ir_input.o ir_sprite.o ir_particle.o ir_asset.o ir_audio.o ir_physics.o ir_entity.o ir_plugin.o $(CJSON_DIR)/cJSON.o $(TOML_DIR)/toml.o -lm $(HARFBUZZ_LIBS)

# Install headers
install-headers: $(BUILD_DIR)
	cp $(IR_HEADERS) $(BUILD_DIR)/

# Clean build artifacts
clean:
	rm -f *.o
	rm -f $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Build test
test: $(IR_STATIC_LIB)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -L$(BUILD_DIR) -lkryon_ir test_ir.c -o $(BUILD_DIR)/test_ir
	$(BUILD_DIR)/test_ir

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

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(IR_STATIC_LIB) $(IR_SHARED_LIB)

# Debug build with memory tracing (max verbosity)
debug-mem: CFLAGS += -g -DDEBUG -DIR_DEBUG_LEVEL=4
debug-mem: clean all

# Sanitizer builds for catching bugs during testing
asan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -O2 -fPIC -g -fsanitize=address -fno-omit-frame-pointer -DDEBUG"

ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -O2 -fPIC -g -fsanitize=undefined -fno-omit-frame-pointer -DDEBUG"

asan-ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -O2 -fPIC -g -fsanitize=address,undefined -fno-omit-frame-pointer -DDEBUG"

tsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -O2 -fPIC -g -fsanitize=thread -fno-omit-frame-pointer -DDEBUG"

# Help
help:
	@echo "Kryon IR System Build Targets:"
	@echo "  all                - Build static and shared libraries"
	@echo "  static             - Build static library only"
	@echo "  shared             - Build shared library only"
	@echo "  test               - Build and run test program"
	@echo "  test-vm            - Build and run VM prototype test"
	@echo "  test-vm-integration - Build and run VM integration test (Phase 1)"
	@echo "  test-event-metadata - Build and run event metadata test (Phase 2)"
	@echo "  clean              - Remove build artifacts"
	@echo "  install-headers    - Copy headers to build directory"
	@echo "  debug              - Build with debug symbols"
	@echo "  debug-mem          - Build with debug symbols and memory tracing"
	@echo "  asan               - Build with AddressSanitizer"
	@echo "  ubsan              - Build with UndefinedBehaviorSanitizer"
	@echo "  asan-ubsan         - Build with both ASan and UBSan (recommended)"
	@echo "  tsan               - Build with ThreadSanitizer"
	@echo "  help               - Show this help"

.PHONY: all clean test test-vm test-vm-integration test-event-metadata debug install-headers help asan ubsan asan-ubsan tsan