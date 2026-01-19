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
INCLUDE_IR_DIR = ../../ir/include
INCLUDE_PUBLIC_DIR = ../../include

# Source files
WEB_SOURCES = ir_web_renderer.c html_generator.c css_generator.c style_analyzer.c lua_bundler.c css_property_tables.c css_generator_core.c css_class_names.c
WEB_HEADERS = ir_web_renderer.h html_generator.h css_generator.h style_analyzer.h lua_bundler.h css_property_tables.h css_generator_core.h css_class_names.h

# Output files
WEB_STATIC_LIB = $(BUILD_DIR)/libkryon_web.a
WEB_SHARED_LIB = $(BUILD_DIR)/libkryon_web.so
WEB_TEST = $(BUILD_DIR)/web_test
KIR_TO_HTML = kir_to_html

# Include paths
INCLUDES = -I$(INCLUDE_DIR) -I$(INCLUDE_IR_DIR) -I$(INCLUDE_PUBLIC_DIR) -I../../ir/src/style -I../../ir/src/utils

# Link libraries
HARFBUZZ_LIBS = $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || echo "")
LINK_LIBS = -L$(BUILD_DIR) -lkryon_web -lkryon_ir $(HARFBUZZ_LIBS)

# Default target
all: $(WEB_STATIC_LIB) $(WEB_SHARED_LIB) $(KIR_TO_HTML)

# Static library only (for embedding in CLI)
static: $(WEB_STATIC_LIB)

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

style_analyzer.o: style_analyzer.c style_analyzer.h
	$(CC) $(CFLAGS) $(INCLUDES) -c style_analyzer.c -o style_analyzer.o

lua_bundler.o: lua_bundler.c lua_bundler.h
	$(CC) $(CFLAGS) $(INCLUDES) -c lua_bundler.c -o lua_bundler.o

css_property_tables.o: css_property_tables.c css_property_tables.h
	$(CC) $(CFLAGS) $(INCLUDES) -c css_property_tables.c -o css_property_tables.o

css_generator_core.o: css_generator_core.c css_generator_core.h
	$(CC) $(CFLAGS) $(INCLUDES) -c css_generator_core.c -o css_generator_core.o

css_class_names.o: css_class_names.c css_class_names.h
	$(CC) $(CFLAGS) $(INCLUDES) -c css_class_names.c -o css_class_names.o

# Create static library
$(WEB_STATIC_LIB): $(BUILD_DIR) ir_web_renderer.o html_generator.o css_generator.o style_analyzer.o lua_bundler.o css_property_tables.o css_generator_core.o css_class_names.o
	$(AR) $(ARFLAGS) $@ ir_web_renderer.o html_generator.o css_generator.o style_analyzer.o lua_bundler.o css_property_tables.o css_generator_core.o css_class_names.o

# Create shared library (dynamically link libkryon_ir to avoid symbol conflicts)
$(WEB_SHARED_LIB): $(BUILD_DIR) ir_web_renderer.o html_generator.o css_generator.o style_analyzer.o lua_bundler.o css_property_tables.o css_generator_core.o css_class_names.o
	$(CC) $(CFLAGS) -shared -o $@ ir_web_renderer.o html_generator.o css_generator.o style_analyzer.o lua_bundler.o css_property_tables.o css_generator_core.o css_class_names.o -L$(BUILD_DIR) -lkryon_ir $(HARFBUZZ_LIBS)

# Build kir_to_html CLI tool
$(KIR_TO_HTML): $(WEB_STATIC_LIB) kir_to_html.c
	$(CC) $(CFLAGS) $(INCLUDES) kir_to_html.c -o $@ $(LINK_LIBS)

# Test and example targets (when test files are created)
# test: $(WEB_TEST)
# example: $(WEB_STATIC_LIB)

# Clean build artifacts
clean:
	rm -f *.o
	rm -f $(WEB_STATIC_LIB) $(WEB_SHARED_LIB) $(WEB_TEST) $(BUILD_DIR)/example_render $(KIR_TO_HTML) css_property_tables.o css_generator_core.o css_class_names.o

# Install headers
install-headers: $(BUILD_DIR)
	cp $(WEB_HEADERS) $(BUILD_DIR)/

# Install vendor files (Fengari, etc.) to user data directory
INSTALL_DIR ?= $(HOME)/.local/share/kryon
install-vendor:
	mkdir -p $(INSTALL_DIR)/vendor
	cp vendor/fengari-web.min.js $(INSTALL_DIR)/vendor/
	@echo "Installed vendor files to $(INSTALL_DIR)/vendor/"

# Full install
install: $(WEB_STATIC_LIB) $(WEB_SHARED_LIB) $(KIR_TO_HTML) install-headers install-vendor
	@echo "Web codegen installed"

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

.PHONY: all static clean debug asan ubsan asan-ubsan tsan install-headers help