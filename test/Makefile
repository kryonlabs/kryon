# Test Makefile for Kryon C Core

# Configuration
TARGET ?= desktop
CORE_DIR = ../core
BUILD_DIR = ../build
TEST_BUILD_DIR = $(BUILD_DIR)/test

# Compiler and flags
CC ?= gcc
CFLAGS = -std=c99 -Wall -Wextra -I$(CORE_DIR)/include
LDFLAGS = -L$(BUILD_DIR) -lkryon_core -lm

# Platform-specific configuration
ifeq ($(TARGET),desktop)
    CFLAGS += -DKRYON_TARGET_PLATFORM=0
    TEST_BINARY = $(TEST_BUILD_DIR)/test_core
endif

ifeq ($(TARGET),mcu)
    CROSS_PREFIX ?= arm-none-eabi-
    CC = $(CROSS_PREFIX)gcc
    CFLAGS += -DKRYON_TARGET_PLATFORM=1 -DKRYON_NO_HEAP=1 -DKRYON_NO_FLOAT=1
    CFLAGS += -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16
    TEST_BINARY = $(TEST_BUILD_DIR)/test_core.elf
    LDFLAGS += --specs=nosys.specs --specs=nano.specs
endif

ifeq ($(TARGET),web)
    CC = emcc
    CFLAGS += -DKRYON_TARGET_PLATFORM=2
    TEST_BINARY = $(TEST_BUILD_DIR)/test_core.js
    LDFLAGS += -s WASM=1
endif

# Source files
TEST_SOURCES = test_core.c
CORE_SOURCES = $(wildcard $(CORE_DIR)/*.c)
OBJECTS = $(TEST_SOURCES:%.c=$(TEST_BUILD_DIR)/%.o)

# Targets
.PHONY: all clean test run

all: $(TEST_BINARY)

$(TEST_BINARY): $(OBJECTS) | $(TEST_BUILD_DIR)
	@echo "Linking test binary: $@"
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

$(TEST_BUILD_DIR)/%.o: %.c | $(TEST_BUILD_DIR)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BUILD_DIR):
	mkdir -p $(TEST_BUILD_DIR)

# Build core library first
$(BUILD_DIR)/libkryon_core.a:
	$(MAKE) -C $(CORE_DIR) TARGET=$(TARGET) static

# Dependencies
$(OBJECTS): $(BUILD_DIR)/libkryon_core.a

test: $(TEST_BINARY)
	@echo "Running tests for target: $(TARGET)"
ifeq ($(TARGET),web)
	@echo "Web target - run with node or browser"
	@echo "Usage: node $<"
else
	./$(TEST_BINARY)
endif

run: test

clean:
	@echo "Cleaning test artifacts"
	rm -rf $(TEST_BUILD_DIR)

help:
	@echo "Kryon C Core Test Makefile"
	@echo "Usage: make [target] TARGET=[platform]"
	@echo ""
	@echo "Targets:"
	@echo "  all   - Build test binary"
	@echo "  test  - Build and run tests"
	@echo "  run   - Alias for test"
	@echo "  clean - Clean test artifacts"
	@echo "  help  - Show this help"
	@echo ""
	@echo "Platforms: desktop, mcu, web"
	@echo ""
	@echo "Examples:"
	@echo "  make TARGET=desktop test  # Run desktop tests"
	@echo "  make TARGET=mcu test      # Run MCU tests"