# Kryon CLI Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I./include -I./src -I../ir -I../third_party/cJSON -I../backends/desktop -D_POSIX_C_SOURCE=200809L
# Desktop backend is linked for hot reload support (--watch flag)
# Requires SDL3 and raylib to be installed
# Link against static libraries to override weak symbols at link time
# Note: libraries specified multiple times to resolve internal dependencies
LDFLAGS = -L../build -lkryon_ir \
	../build/libkryon_common.a \
	../build/libkryon_sdl3.a \
	../build/libkryon_desktop.a \
	../build/libkryon_common.a \
	../build/libkryon_sdl3.a \
	../build/libkryon_desktop.a \
	-lm -lpthread -ldl -lSDL3 -lSDL3_ttf -lraylib

# Directories
SRC_DIR = src
BUILD_DIR = build
INSTALL_DIR = $(HOME)/.local/bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/utils/string.c \
       $(SRC_DIR)/utils/file.c \
       $(SRC_DIR)/utils/file_discovery.c \
       $(SRC_DIR)/utils/file_watcher.c \
       $(SRC_DIR)/utils/ws_reload.c \
       $(SRC_DIR)/utils/args.c \
       $(SRC_DIR)/utils/process.c \
       $(SRC_DIR)/utils/json.c \
       $(SRC_DIR)/utils/dev_server.c \
       $(SRC_DIR)/utils/paths.c \
       $(SRC_DIR)/utils/build.c \
       $(SRC_DIR)/config/toml_parser.c \
       $(SRC_DIR)/config/config.c \
       $(SRC_DIR)/template/docs_template.c \
       $(SRC_DIR)/commands/cmd_stubs.c \
       $(SRC_DIR)/commands/cmd_config.c \
       $(SRC_DIR)/commands/cmd_build.c \
       $(SRC_DIR)/commands/cmd_compile.c \
       $(SRC_DIR)/commands/cmd_run.c \
       $(SRC_DIR)/commands/cmd_codegen.c \
       $(SRC_DIR)/commands/cmd_new.c \
       $(SRC_DIR)/commands/cmd_inspect.c \
       $(SRC_DIR)/commands/cmd_diff.c \
       $(SRC_DIR)/commands/cmd_plugin.c \
       $(SRC_DIR)/commands/cmd_install.c \
       $(SRC_DIR)/commands/cmd_uninstall.c \
       $(SRC_DIR)/build/luajit_compile.c \
       $(SRC_DIR)/build/embed_bytecode.c \
       $(SRC_DIR)/build/plugin_discovery.c \
       $(SRC_DIR)/build/luajit_link.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Target binary
TARGET = kryon

# Codegen object files (only include existing ones)
CODEGEN_OBJS = $(wildcard ../codegens/kry/kry_codegen.o) \
               $(wildcard ../codegens/tsx/tsx_codegen.o) \
               $(wildcard ../codegens/nim/nim_codegen.o) \
               $(wildcard ../codegens/lua/lua_codegen.o) \
               $(wildcard ../codegens/markdown/markdown_codegen.o) \
               $(wildcard ../codegens/c/ir_c_codegen.o) \
               $(wildcard ../codegens/kotlin/kotlin_codegen.o) \
               $(wildcard ../codegens/react_common.o)

# Third-party libraries
CJSON_OBJ = $(BUILD_DIR)/cJSON.o

# Default target
all: $(TARGET)

# Minimal target - no SDL3/desktop dependencies (for terminal/web builds)
CFLAGS_MINIMAL = -std=c99 -Wall -Wextra -O2 -I./include -I./src -I../ir -I../third_party/cJSON -DKRYON_MINIMAL_BUILD -D_POSIX_C_SOURCE=200809L
OBJS_MINIMAL = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
minimal: $(BUILD_DIR) $(CJSON_OBJ) ../build/libkryon_ir.a ../build/libkryon_common.a
	$(MAKE) -C . CFLAGS="$(CFLAGS_MINIMAL)" $(OBJS_MINIMAL)
	$(CC) $(OBJS_MINIMAL) $(CODEGEN_OBJS) $(CJSON_OBJ) -o $(TARGET) \
		-L../build -lkryon_ir \
		../build/libkryon_common.a \
		../build/libkryon_common.a \
		-lm -lpthread -ldl

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/utils
	mkdir -p $(BUILD_DIR)/config
	mkdir -p $(BUILD_DIR)/template
	mkdir -p $(BUILD_DIR)/commands
	mkdir -p $(BUILD_DIR)/build
	mkdir -p $(BUILD_DIR)/plugin
	mkdir -p $(BUILD_DIR)/codegen

# Build third-party libraries
$(CJSON_OBJ): ../third_party/cJSON/cJSON.c
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build target (depends on IR and desktop libraries)
$(TARGET): $(BUILD_DIR) $(OBJS) $(CJSON_OBJ) \
		../build/libkryon_ir.a \
		../build/libkryon_common.a \
		../build/libkryon_sdl3.a \
		../build/libkryon_desktop.a
	$(CC) $(OBJS) $(CODEGEN_OBJS) $(CJSON_OBJ) -o $(TARGET) $(LDFLAGS)

# Build IR library
../build/libkryon_ir.a:
	$(MAKE) -C ../ir all

# Build common renderer library
../build/libkryon_common.a:
	$(MAKE) -C ../renderers/common all
	$(MAKE) -C ../renderers/common install

# Build SDL3 renderer library
../build/libkryon_sdl3.a:
	$(MAKE) -C ../renderers/sdl3 all

# Build desktop library
../build/libkryon_desktop.a:
	$(MAKE) -C ../backends/desktop all

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Install
install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	chmod +x $(INSTALL_DIR)/$(TARGET)
	# Build and install IR library (needed for Lua runtime)
	$(MAKE) -C ../ir all
	mkdir -p $(HOME)/.local/share/kryon/build
	cp ../build/libkryon_ir.so $(HOME)/.local/share/kryon/build/
	# Also install to .local/lib for dynamic linker
	mkdir -p $(HOME)/.local/lib
	cp ../build/libkryon_ir.so $(HOME)/.local/lib/
	# Install TSX parser
	mkdir -p $(HOME)/.local/share/kryon/tsx_parser
	cp ../ir/parsers/tsx/tsx_to_kir.ts $(HOME)/.local/share/kryon/tsx_parser/
	# Install Lua bindings
	mkdir -p $(HOME)/.local/share/kryon/bindings/lua
	cp -r ../bindings/lua/* $(HOME)/.local/share/kryon/bindings/lua/
	# Install C bindings
	mkdir -p $(HOME)/.local/share/kryon/bindings/c
	cp -r ../bindings/c/* $(HOME)/.local/share/kryon/bindings/c/
	# Build and install web codegen
	$(MAKE) -C ../codegens/web kir_to_html
	mkdir -p $(HOME)/.local/share/kryon/bin
	cp ../codegens/web/kir_to_html $(HOME)/.local/share/kryon/bin/
	chmod +x $(HOME)/.local/share/kryon/bin/kir_to_html
	# Install dev server script
	mkdir -p $(HOME)/.local/share/kryon/scripts
	cp scripts/dev_server.ts $(HOME)/.local/share/kryon/scripts/
	# Install luajit_runtime.c template for building Lua binaries
	mkdir -p $(HOME)/.local/share/kryon/cli/src/build
	cp src/build/luajit_runtime.c $(HOME)/.local/share/kryon/cli/src/build/

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild
rebuild: clean all

.PHONY: all clean install rebuild
