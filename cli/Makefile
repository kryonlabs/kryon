# Kryon CLI Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I./include -I../ir -I../ir/third_party/cJSON -D_POSIX_C_SOURCE=200809L
LDFLAGS = -L../build -lkryon_ir -lkryon_desktop -lm ../ir/third_party/cJSON/cJSON.c

# Directories
SRC_DIR = src
BUILD_DIR = build
INSTALL_DIR = $(HOME)/.local/bin

# Source files
SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/utils/string.c \
       $(SRC_DIR)/utils/file.c \
       $(SRC_DIR)/utils/args.c \
       $(SRC_DIR)/utils/process.c \
       $(SRC_DIR)/utils/json.c \
       $(SRC_DIR)/config/toml_parser.c \
       $(SRC_DIR)/config/config.c \
       $(SRC_DIR)/commands/cmd_stubs.c \
       $(SRC_DIR)/commands/cmd_config.c \
       $(SRC_DIR)/commands/cmd_build.c \
       $(SRC_DIR)/commands/cmd_compile.c \
       $(SRC_DIR)/commands/cmd_run.c \
       $(SRC_DIR)/commands/cmd_codegen.c \
       $(SRC_DIR)/commands/cmd_new.c \
       $(SRC_DIR)/commands/cmd_inspect.c \
       $(SRC_DIR)/commands/cmd_diff.c

# Object files
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

# Target binary
TARGET = kryon

# Codegen object files
CODEGEN_OBJS = ../codegens/kry/kry_codegen.o ../codegens/tsx/tsx_codegen.o ../codegens/nim/nim_codegen.o ../codegens/lua/lua_codegen.o ../codegens/markdown/markdown_codegen.o ../codegens/c/ir_c_codegen.o ../codegens/kotlin/kotlin_codegen.o ../codegens/react_common.o

# Default target
all: $(TARGET)

# Create build directories
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)/utils
	mkdir -p $(BUILD_DIR)/config
	mkdir -p $(BUILD_DIR)/commands
	mkdir -p $(BUILD_DIR)/build
	mkdir -p $(BUILD_DIR)/plugin
	mkdir -p $(BUILD_DIR)/codegen

# Build target
$(TARGET): $(BUILD_DIR) $(OBJS)
	$(CC) $(OBJS) $(CODEGEN_OBJS) -o $(TARGET) $(LDFLAGS)

# Compile source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# Install
install: $(TARGET)
	mkdir -p $(INSTALL_DIR)
	cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	chmod +x $(INSTALL_DIR)/$(TARGET)

# Clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

# Rebuild
rebuild: clean all

.PHONY: all clean install rebuild
