# Functional Tests Makefile
# End-to-end tests (.kyt files for UI testing)

# Directories
BUILD_DIR = ../../build
TEST_BIN_DIR = $(BUILD_DIR)/test-binaries

# Default target
all: $(TEST_BIN_DIR)

# Create test binary directory
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Run functional tests
test: all
	@echo "Running functional tests..."
	@echo ""
	@echo "Note: .kyt functional test runner not yet fully implemented"
	@echo "Functional tests are in:"
	@echo "  - interactive/  (UI interaction tests)"
	@echo "  - plugins/      (plugin tests)"
	@echo ""
	@echo "Example test files:"
	@echo "  interactive/tabs_switching.kyt"
	@echo ""
	@echo "To run a specific test manually:"
	@echo "  kryon test tests/functional/interactive/tabs_switching.kyt"

# Clean
clean:
	rm -rf $(TEST_BIN_DIR)

.PHONY: all test clean
