# Unit Tests Makefile
# Delegates to subdirectory test suites

# Test subdirectories (codegens subdirectories have their own Makefiles)
TEST_SUBDIRS := ir parsers

# Default target: build all tests
all:
	@for dir in $(TEST_SUBDIRS); do \
		$(MAKE) -C $$dir all || exit 1; \
	done

# Run all unit tests
test: all
	@echo "Running unit tests..."
	@for dir in $(TEST_SUBDIRS); do \
		echo ""; \
		echo "=== Running $$dir tests ==="; \
		$(MAKE) -C $$dir test || exit 1; \
	done
	@echo ""
	@echo "✓ Unit tests passed!"

# Clean all test artifacts
clean:
	@for dir in $(TEST_SUBDIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done

.PHONY: all test clean
