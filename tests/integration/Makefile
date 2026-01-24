# Integration Tests Makefile
# Tests component interactions

# Directories
BUILD_DIR = ../../build
TEST_BIN_DIR = $(BUILD_DIR)/test-binaries

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -I../../ir/include -I../../include -I../../codegens/hare
LDFLAGS = -L$(BUILD_DIR) -lkryon_ir -lm

# Default target
all: $(TEST_BIN_DIR)

# Create test binary directory
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Hare Pipeline Integration Tests
test_hare_pipeline: test_hare_pipeline.c
	$(CC) $(CFLAGS) $< $(LDFLAGS) -o $(TEST_BIN_DIR)/$@

# Run integration tests
test: all test_hare_pipeline
	@echo ""
	@echo "╔══════════════════════════════════════════════╗"
	@echo "║     Running Integration Tests                ║"
	@echo "╚══════════════════════════════════════════════╝"
	@echo ""
	$(TEST_BIN_DIR)/test_hare_pipeline

# Clean
clean:
	rm -rf $(TEST_BIN_DIR)

.PHONY: all test clean
