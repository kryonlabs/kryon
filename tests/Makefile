# Test Makefile for Kryon KIR System

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -I../include -I../src
LDFLAGS = -lm

# Test binaries
TEST_BINS = compiler/test_kir_roundtrip

# Build directory
BUILD_DIR = build

.PHONY: all clean run

all: $(BUILD_DIR) $(addprefix $(BUILD_DIR)/,$(TEST_BINS))

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)/compiler

$(BUILD_DIR)/compiler/test_kir_roundtrip: compiler/test_kir_roundtrip.c
	@echo "[CC] $<"
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	@echo "[CLEAN] tests/$(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)
	@rm -rf fixtures output

run: all
	@echo "Running KIR round-trip tests..."
	@cd .. && tests/$(BUILD_DIR)/compiler/test_kir_roundtrip

.PHONY: all clean run
