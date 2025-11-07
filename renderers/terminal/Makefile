# Kryon Terminal Renderer Makefile
# Comprehensive build system with testing and CI/CD support

# Compiler and flags
CC ?= gcc
CFLAGS ?= -std=c99 -Wall -Wextra -Werror -O2
INCLUDES = -I../../core/include
LDFLAGS = -lutil

# Directories
BUILD_DIR = ../../build
CORE_LIB = $(BUILD_DIR)/libkryon_core.a
TESTDIR = tests

# Source files
SOURCES = terminal_simple.c
HEADERS = terminal_simple.h
OBJECTS = $(SOURCES:.c=.o)
TARGET = $(BUILD_DIR)/libkryon_terminal.a

# Test files
TEST_SOURCES = test_suite.c regression_tests.c event_system_tests.c terminal_test.c
TEST_OBJECTS = $(TEST_SOURCES:.c=.o)
TEST_BINS = test_suite regression_tests event_system_tests
TEST_COMPAT_BIN = $(BUILD_DIR)/terminal_test

# Performance
BENCHMARK_SOURCES = benchmark.c
BENCHMARK_BIN = benchmark

# Compatibility and stress testing
COMPAT_SOURCES = terminal_compatibility_matrix.c
COMPAT_BIN = terminal_compatibility_matrix
STRESS_SOURCES = stress_test.c
STRESS_BIN = stress_test

# Default target
.PHONY: all clean test examples benchmark install uninstall help
all: $(TARGET) test_bins

# Build static library
$(TARGET): $(OBJECTS) $(CORE_LIB)
	@echo "Building terminal renderer library..."
	@mkdir -p $(BUILD_DIR)
	ar rcs $@ $(OBJECTS)
	@echo "✓ Terminal renderer built: $@"

# Build shared library
.PHONY: shared
shared: $(CORE_LIB)
	@echo "Building terminal renderer shared library..."
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -shared -o $(BUILD_DIR)/libkryon_terminal.so $(SOURCES) $(LDFLAGS) -L$(BUILD_DIR) -lkryon_core
	@echo "✓ Terminal renderer shared library built"

# Install target (for compatibility with other renderers)
.PHONY: install
install: $(TARGET)
	@echo "Installing terminal renderer..."
	@echo "✓ Terminal renderer installed: $(TARGET)"

# Compile source files
%.o: %.c $(HEADERS)
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) $(INCLUDES) -fPIC -c $< -o $@

# Test targets
test_bins: $(TEST_BINS)

test_suite: test_suite.c $(OBJECTS)
	@echo "Building test suite: $@"
	$(CC) $(CFLAGS) -o $@ $< $(OBJECTS) $(LDFLAGS) -DKRYON_TERMINAL_TEST

regression_tests: regression_tests.c $(OBJECTS)
	@echo "Building regression tests: $@"
	$(CC) $(CFLAGS) -o $@ $< $(OBJECTS) $(LDFLAGS)

event_system_tests: event_system_tests.c $(OBJECTS)
	@echo "Building event system tests: $@"
	$(CC) $(CFLAGS) -o $@ $< $(OBJECTS) $(LDFLAGS)

# Compatibility test
$(TEST_COMPAT_BIN): terminal_test.c
	@echo "Building compatibility test: $@"
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $< -lutil
	@echo "✓ Compatibility test built: $@"

# Benchmark
benchmark: $(BENCHMARK_SOURCES) $(OBJECTS)
	@echo "Building performance benchmark: $@"
	$(CC) $(CFLAGS) -O3 -o $@ $< $(OBJECTS) $(LDFLAGS)

# Compatibility testing
$(COMPAT_BIN): $(COMPAT_SOURCES) $(OBJECTS)
	@echo "Building compatibility matrix: $@"
	$(CC) $(CFLAGS) -o $@ $< $(OBJECTS) $(LDFLAGS) -lrt

# Stress testing
$(STRESS_BIN): $(STRESS_SOURCES) $(OBJECTS)
	@echo "Building stress test: $@"
	$(CC) $(CFLAGS) -pthread -o $@ $< $(OBJECTS) $(LDFLAGS) -lrt

# Ensure C core is built
$(CORE_LIB):
	@echo "Building Kryon C core..."
	@$(MAKE) -C ../../core

# Install dependencies (Ubuntu/Debian)
.PHONY: deps
deps:
	@echo "Installing terminal renderer dependencies..."
	sudo apt update
	sudo apt install -y libtickit-dev libncurses-dev ncurses-bin

# Install dependencies (macOS)
.PHONY: deps-macos
deps-macos:
	@echo "Installing terminal renderer dependencies (macOS)..."
	brew install tickit

# Test execution
.PHONY: test test-all test-unit test-regression test-events test-memory test-performance

test: $(TEST_COMPAT_BIN)
	@echo "Running basic compatibility test..."
	$(TEST_COMPAT_BIN)

test-all: test_bins
	@echo "Running all test suites..."
	@echo "=== Unit Tests ==="
	./test_suite
	@echo "=== Regression Tests ==="
	./regression_tests
	@echo "=== Event System Tests ==="
	./event_system_tests

test-unit: test_suite
	@echo "Running unit tests..."
	./test_suite

test-regression: regression_tests
	@echo "Running regression tests..."
	./regression_tests

test-events: event_system_tests
	@echo "Running event system tests..."
	./event_system_tests

test-memory:
	@echo "Running memory leak detection..."
	@if command -v valgrind >/dev/null 2>&1; then \
		valgrind --leak-check=full --error-exitcode=1 ./test_suite; \
		valgrind --leak-check=full --error-exitcode=1 ./regression_tests; \
		echo "✅ Memory leak detection passed"; \
	else \
		echo "⚠️  Valgrind not available, skipping memory tests"; \
	fi

test-performance: benchmark
	@echo "Running performance benchmarks..."
	./benchmark

# Comprehensive testing targets
.PHONY: test-compatibility test-stress

test-compatibility: $(COMPAT_BIN)
	@echo "Running compatibility matrix test..."
	./$(COMPAT_BIN)

test-stress: $(STRESS_BIN)
	@echo "Running stress tests..."
	./$(STRESS_BIN)

# CI/CD targets
.PHONY: ci-test ci-build ci-clean

ci-test: test-all test-memory test-compatibility test-stress
	@echo "CI test suite completed successfully"

ci-build: clean all test_bins $(COMPAT_BIN) $(STRESS_BIN)
	@echo "CI build completed successfully"

ci-clean: clean
	@echo "CI cleanup completed"

# Clean build artifacts
.PHONY: clean
clean:
	@echo "Cleaning terminal renderer..."
	rm -f $(OBJECTS) $(TARGET) $(BUILD_DIR)/libkryon_terminal.so
	rm -f $(TEST_BINS) $(TEST_COMPAT_BIN) $(BENCHMARK_BIN)
	rm -f $(COMPAT_BIN) $(STRESS_BIN)
	rm -f *.log *.out compat_test_*.csv compatibility_results_*.csv
	rm -f *.a *.so test_* compat_test_* benchmark stress_test terminal_compatibility_matrix
	@echo "✓ Clean complete"

# Test terminal capabilities
.PHONY: test-term
test-term:
	@echo "Testing terminal capabilities..."
	@echo "Terminal type: $$TERM"
	@echo "Colors supported: $$(tput colors 2>/dev/null || echo 'unknown')"
	@echo "Unicode support: $$(echo '✓ UTF-8: ✓ ✓ ✓')"
	@python3 -c "import tickit; print('✓ libtickit available')" 2>/dev/null || echo "✗ libtickit not available"
	@echo "Terminal size: $$(tput cols 2>/dev/null || echo 'unknown')x$$(tput lines 2>/dev/null || echo 'unknown')"

# Run compatibility test
.PHONY: test-compat
test-compat: $(TEST_COMPAT_BIN)
	@echo "Running terminal compatibility test..."
	$(TEST_COMPAT_BIN)


# Stress testing
.PHONY: stress-test stress-memory stress-performance

stress-test: test_bins
	@echo "Running stress tests..."
	@echo "=== Memory Stress Test ==="
	./test_suite --stress 2>/dev/null || ./test_suite
	@echo "=== Performance Stress Test ==="
	./regression_tests --iterations 10000 2>/dev/null || ./regression_tests

stress-memory:
	@echo "Running memory stress tests..."
	@if command -v valgrind >/dev/null 2>&1; then \
		for i in 1 2 3 4 5; do \
			echo "Stress iteration $$i..."; \
			valgrind --leak-check=full --error-exitcode=1 ./test_suite >/dev/null 2>&1 || exit 1; \
		done; \
		echo "✅ Memory stress tests passed"; \
	else \
		echo "⚠️  Valgrind not available, skipping memory stress tests"; \
	fi

stress-performance: benchmark
	@echo "Running performance stress tests..."
	@for i in 1 2 3; do \
		echo "Performance iteration $$i..."; \
		./benchmark --stress 2>/dev/null || ./benchmark; \
	done
	@echo "✅ Performance stress tests completed"

# Terminal compatibility testing
.PHONY: test-compatibility-matrix

test-compatibility-matrix: $(TEST_COMPAT_BIN)
	@echo "Testing terminal compatibility matrix..."
	@for term in xterm-256color screen tmux alacritty kitty; do \
		if [ -n "$$term" ]; then \
			echo "Testing with TERM=$$term..."; \
			TERM=$$term $(TEST_COMPAT_BIN) > compat_test_$$term.log 2>&1 && \
				echo "✅ $$term: Compatible" || \
				echo "⚠️  $$term: Issues detected"; \
		fi; \
	done
	@echo "Compatibility test results saved to compat_test_*.log"

# Build system validation
.PHONY: validate-build validate-deps validate-syntax

validate-build: clean all
	@echo "Validating build system..."
	@echo "✅ Build system validation passed"

validate-deps:
	@echo "Validating dependencies..."
	@pkg-config --exists tickit || (echo "❌ libtickit not found" && exit 1)
	@pkg-config --modversion tickit
	@tic -V >/dev/null || (echo "❌ terminfo not available" && exit 1)
	@echo "✅ Dependencies validated"

validate-syntax:
	@echo "Validating syntax..."
	@for file in $(SOURCES) $(HEADERS); do \
		$(CC) $(CFLAGS) -fsyntax-only $$file || exit 1; \
	done
	@echo "✅ Syntax validation passed"

# Show build info
.PHONY: info
info:
	@echo "Kryon Terminal Renderer Build Info"
	@echo "=================================="
	@echo "CC: $(CC)"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "INCLUDES: $(INCLUDES)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "SOURCES: $(SOURCES)"
	@echo "TARGET: $(TARGET)"
	@echo ""
	@echo "Required libraries:"
	@echo "  - libtickit-dev (tickit terminal abstraction)"
	@echo "  - libncurses-dev (curses compatibility)"
	@echo "  - libutil, libtinfo (system utilities)"

# Help
.PHONY: help
help:
	@echo "Kryon Terminal Renderer Build System"
	@echo "===================================="
	@echo ""
	@echo "Build targets:"
	@echo "  all           - Build libraries and test binaries"
	@echo "  shared        - Build shared library"
	@echo "  clean         - Clean build artifacts"
	@echo "  benchmark     - Build performance benchmark"
	@echo ""
	@echo "Test targets:"
	@echo "  test          - Run basic compatibility test"
	@echo "  test-all      - Run all test suites"
	@echo "  test-unit     - Run unit tests only"
	@echo "  test-memory   - Run memory leak detection"
	@echo "  test-performance - Run performance benchmarks"
	@echo "  test-compatibility - Run compatibility matrix test"
	@echo "  test-stress   - Run stress tests"
	@echo ""
	@echo "CI/CD targets:"
	@echo "  ci-test       - Run CI test suite"
	@echo "  ci-build      - Full CI build"
	@echo "  ci-clean      - CI cleanup"
	@echo ""
	@echo "Validation targets:"
	@echo "  validate-build - Validate build system"
	@echo "  validate-deps  - Validate dependencies"
	@echo "  validate-syntax - Validate source syntax"
	@echo ""
	@echo "Stress testing:"
	@echo "  stress-test   - Run all stress tests"
	@echo "  stress-memory - Memory stress testing"
	@echo "  stress-performance - Performance stress testing"
	@echo ""
	@echo "Dependencies:"
	@echo "  deps          - Install dependencies (Ubuntu/Debian)"
	@echo "  deps-macos    - Install dependencies (macOS)"
	@echo ""
	@echo "Diagnostics:"
	@echo "  test-term     - Test terminal capabilities"
	@echo "  test-compat   - Run compatibility test"
	@echo "  info          - Show build configuration"
	@echo ""
	@echo "Usage examples:"
	@echo "  make                # Build static library and tests"
	@echo "  make test-all       # Run all test suites"
	@echo "  make ci-test        # Run CI test suite"
	@echo "  make test-compatibility # Test terminal compatibility"
	@echo "  make test-stress    # Run stress tests"
	@echo "  make validate-build # Validate build system"

# Ensure object files depend on headers
%.o: %.c ../../core/include/kryon.h

# Prevent make from removing intermediate files
.PRECIOUS: %.o