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

# Source files
IR_SOURCES = ir_builder.c ir_serialization.c ir_validation.c ir_reactive_manifest.c ir_style_vars.c ir_layout.c ir_memory.c ir_hashmap.c ir_animation.c ir_text_shaping.c ir_hot_reload.c ir_format_detect.c ir_json_v2.c third_party/cJSON/cJSON.c
IR_HEADERS = ir_core.h ir_builder.h ir_serialization.h ir_validation.h ir_style_vars.h ir_memory.h ir_hashmap.h ir_animation.h ir_text_shaping.h ir_hot_reload.h ir_format_detect.h third_party/cJSON/cJSON.h

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

ir_reactive_manifest.o: ir_reactive_manifest.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_reactive_manifest.c -o ir_reactive_manifest.o

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

third_party/cJSON/cJSON.o: third_party/cJSON/cJSON.c third_party/cJSON/cJSON.h
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c third_party/cJSON/cJSON.c -o third_party/cJSON/cJSON.o

ir_json_v2.o: ir_json_v2.c $(IR_HEADERS)
	$(CC) $(CFLAGS) -I$(INCLUDE_DIR) -c ir_json_v2.c -o ir_json_v2.o

# Create static library
$(IR_STATIC_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_validation.o ir_reactive_manifest.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o third_party/cJSON/cJSON.o
	$(AR) $(ARFLAGS) $@ ir_builder.o ir_serialization.o ir_validation.o ir_reactive_manifest.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o third_party/cJSON/cJSON.o

# Create shared library
$(IR_SHARED_LIB): $(BUILD_DIR) ir_builder.o ir_serialization.o ir_validation.o ir_reactive_manifest.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o third_party/cJSON/cJSON.o
	$(CC) $(CFLAGS) -shared -o $@ ir_builder.o ir_serialization.o ir_validation.o ir_reactive_manifest.o ir_style_vars.o ir_layout.o ir_memory.o ir_hashmap.o ir_animation.o ir_text_shaping.o ir_hot_reload.o ir_format_detect.o ir_json_v2.o third_party/cJSON/cJSON.o -lm $(HARFBUZZ_LIBS)

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
	@echo "  all          - Build static and shared libraries"
	@echo "  static       - Build static library only"
	@echo "  shared       - Build shared library only"
	@echo "  test         - Build and run test program"
	@echo "  clean        - Remove build artifacts"
	@echo "  install-headers - Copy headers to build directory"
	@echo "  debug        - Build with debug symbols"
	@echo "  debug-mem    - Build with debug symbols and memory tracing"
	@echo "  asan         - Build with AddressSanitizer"
	@echo "  ubsan        - Build with UndefinedBehaviorSanitizer"
	@echo "  asan-ubsan   - Build with both ASan and UBSan (recommended)"
	@echo "  tsan         - Build with ThreadSanitizer"
	@echo "  help         - Show this help"

.PHONY: all clean test debug install-headers help asan ubsan asan-ubsan tsan