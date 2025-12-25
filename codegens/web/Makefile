# Makefile for Kryon Web Backend
# Compiles IR to HTML/CSS/JS for web deployment

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
AR = ar
ARFLAGS = rcs

# Directories
WEB_DIR = .
IR_DIR = ../../ir
BUILD_DIR = ../../build
INCLUDE_DIR = .
INCLUDE_IR_DIR = ../../ir

# Source files
WEB_SOURCES = ir_web_renderer.c html_generator.c css_generator.c wasm_bridge.c svg_generator.c
WEB_HEADERS = ir_web_renderer.h html_generator.h css_generator.h wasm_bridge.h svg_generator.h

# Output files
WEB_STATIC_LIB = $(BUILD_DIR)/libkryon_web.a
WEB_SHARED_LIB = $(BUILD_DIR)/libkryon_web.so
WEB_TEST = $(BUILD_DIR)/web_test

# Include paths
INCLUDES = -I$(INCLUDE_DIR) -I$(INCLUDE_IR_DIR)

# Link libraries
LINK_LIBS = -L$(BUILD_DIR) -lkryon_ir

# Default target
all: $(WEB_STATIC_LIB) $(WEB_SHARED_LIB)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Compile object files
ir_web_renderer.o: ir_web_renderer.c $(WEB_HEADERS)
	$(CC) $(CFLAGS) $(INCLUDES) -c ir_web_renderer.c -o ir_web_renderer.o

html_generator.o: html_generator.c html_generator.h
	$(CC) $(CFLAGS) $(INCLUDES) -c html_generator.c -o html_generator.o

css_generator.o: css_generator.c css_generator.h
	$(CC) $(CFLAGS) $(INCLUDES) -c css_generator.c -o css_generator.o

wasm_bridge.o: wasm_bridge.c wasm_bridge.h
	$(CC) $(CFLAGS) $(INCLUDES) -c wasm_bridge.c -o wasm_bridge.o

svg_generator.o: svg_generator.c svg_generator.h
	$(CC) $(CFLAGS) $(INCLUDES) -c svg_generator.c -o svg_generator.o

# Create static library
$(WEB_STATIC_LIB): $(BUILD_DIR) ir_web_renderer.o html_generator.o css_generator.o wasm_bridge.o svg_generator.o
	$(AR) $(ARFLAGS) $@ ir_web_renderer.o html_generator.o css_generator.o wasm_bridge.o svg_generator.o

# Create shared library
$(WEB_SHARED_LIB): $(BUILD_DIR) ir_web_renderer.o html_generator.o css_generator.o wasm_bridge.o svg_generator.o
	$(CC) $(CFLAGS) -shared -o $@ ir_web_renderer.o html_generator.o css_generator.o wasm_bridge.o svg_generator.o ../../build/libkryon_ir.a

# Test and example targets (when test files are created)
# test: $(WEB_TEST)
# example: $(WEB_STATIC_LIB)

# Clean build artifacts
clean:
	rm -f *.o
	rm -f $(WEB_STATIC_LIB) $(WEB_SHARED_LIB) $(WEB_TEST) $(BUILD_DIR)/example_render

# Install headers
install-headers: $(BUILD_DIR)
	cp $(WEB_HEADERS) $(BUILD_DIR)/

# Debug build
debug: CFLAGS += -g -DDEBUG
debug: $(WEB_STATIC_LIB) $(WEB_SHARED_LIB)

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
	@echo "Kryon Web Backend Build Targets:"
	@echo "  all            - Build static and shared libraries"
	@echo "  static         - Build static library only"
	@echo "  shared         - Build shared library only"
	@echo "  clean          - Remove build artifacts"
	@echo "  install-headers - Copy headers to build directory"
	@echo "  debug          - Build with debug symbols"
	@echo "  asan           - Build with AddressSanitizer"
	@echo "  ubsan          - Build with UndefinedBehaviorSanitizer"
	@echo "  asan-ubsan     - Build with both ASan and UBSan (recommended)"
	@echo "  tsan           - Build with ThreadSanitizer"
	@echo "  help           - Show this help"

.PHONY: all clean debug asan ubsan asan-ubsan tsan install-headers help