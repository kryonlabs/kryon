# Kryon Test Suite Makefile
# ===========================
# Unified build and execution for all test suites

# Load shared configuration
include ../build/config.mk

# Test directories
TEST_DIRS := unit integration functional
BUILD_DIR := ../build

# Test binary output directory
TEST_BIN_DIR := $(BUILD_DIR)/test-binaries

# Default target
all: $(TEST_DIRS)

# Run all tests
test: all
	@echo "Running all test suites..."
	@for dir in $(TEST_DIRS); do \
		echo ""; \
		echo "=== Running $$dir tests ==="; \
		$(MAKE) -C $$dir test || exit 1; \
	done
	@echo ""
	@echo "✓ All tests passed!"

# Build all test binaries
$(TEST_DIRS):
	@echo "Building $$dir tests..."
	@$(MAKE) -C $@

# Clean all test artifacts
clean:
	@echo "Cleaning test artifacts..."
	@for dir in $(TEST_DIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done
	@rm -rf $(TEST_BIN_DIR)
	@echo "✓ Test artifacts cleaned"

# Run specific test category
test-unit:
	@$(MAKE) -C unit test

test-integration:
	@$(MAKE) -C integration test

test-functional:
	@$(MAKE) -C functional test

# Run tests with coverage (if gcov is available)
test-coverage:
	@echo "Running tests with coverage analysis..."
	@if command -v gcov >/dev/null 2>&1; then \
		CFLAGS="--coverage" LDFLAGS="--coverage" $(MAKE) test; \
		gcov ../ir/src/**/*.gcno | grep -E "(File|Lines|Branches)" || true; \
	else \
		echo "gcov not found, skipping coverage"; \
	fi

# Run tests under valgrind (if available)
test-memcheck:
	@echo "Running tests with valgrind..."
	@if command -v valgrind >/dev/null 2>&1; then \
		for test in $(TEST_BIN_DIR)/*; do \
			echo "Testing $$test..."; \
			valgrind --leak-check=full --error-exitcode=1 $$test || exit 1; \
		done; \
	else \
		echo "valgrind not found, skipping memcheck"; \
	fi

# Help
help-test:
	@echo "Kryon Test Suite"
	@echo ""
	@echo "Targets:"
	@echo "  all            - Build all test binaries"
	@echo "  test           - Run all tests"
	@echo "  test-unit      - Run unit tests only"
	@echo "  test-integration - Run integration tests only"
	@echo "  test-functional  - Run functional tests only"
	@echo "  test-coverage  - Run tests with coverage analysis"
	@echo "  test-memcheck   - Run tests with valgrind memcheck"
	@echo "  clean          - Clean test artifacts"
	@echo ""
	@echo "Test Categories:"
	@echo "  unit           - Unit tests for individual components"
	@echo "  integration    - Integration tests for component interactions"
	@echo "  functional     - End-to-end functional tests"

.PHONY: all test clean test-unit test-integration test-functional
.PHONY: test-coverage test-memcheck help-test
