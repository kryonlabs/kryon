# Integration Tests Makefile
# Tests component interactions

# Directories
BUILD_DIR = ../../build
TEST_BIN_DIR = $(BUILD_DIR)/test-binaries

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -I../../ir/include -I../../include
LDFLAGS = -L$(BUILD_DIR) -lkryon_ir -lm

# Default target
all: $(TEST_BIN_DIR)

# Create test binary directory
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Run integration tests
test: all
	@echo "No integration tests yet"
	@echo "Integration tests verify component interactions"
	@echo "Examples: parser → IR → codegen, frontend → backend, etc."

# Clean
clean:
	rm -rf $(TEST_BIN_DIR)

.PHONY: all test clean
