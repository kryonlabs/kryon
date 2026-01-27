# Parser Tests Makefile
# Delegates to parser-specific test subdirectories

TEST_SUBDIRS :=

all:
	@for dir in $(TEST_SUBDIRS); do \
		$(MAKE) -C $$dir all || exit 1; \
	done

test: all
	@echo "Running parser tests..."
	@for dir in $(TEST_SUBDIRS); do \
		echo ""; \
		echo "=== Running $$dir parser tests ==="; \
		$(MAKE) -C $$dir test || exit 1; \
	done
	@echo ""
	@echo "✓ Parser tests passed!"

clean:
	@for dir in $(TEST_SUBDIRS); do \
		$(MAKE) -C $$dir clean || true; \
	done

.PHONY: all test clean
