# Makefile for Kryon IR Unit Tests

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -g -I.. -DDEBUG
LDFLAGS = -L../../build -lkryon_ir -lm

# HarfBuzz, FreeType, and FriBidi (needed by ir_text_shaping)
HARFBUZZ_LIBS = $(shell pkg-config --libs harfbuzz freetype2 fribidi 2>/dev/null || \
	echo "-L/nix/store/8mk1lafnnrx0a6lvjh69afm8w3i4pvqx-harfbuzz-12.1.0/lib \
	-L/nix/store/p1cqrricl0ch7qab8b8w3krhc91d890v-freetype-2.13.3/lib \
	-L/nix/store/62h5sfxr4s1qc3fghcan06viq4gms6yg-fribidi-1.0.16/lib \
	-lharfbuzz -lfreetype -lfribidi")

# Directories
BUILD_DIR = ../../build
TEST_BIN_DIR = $(BUILD_DIR)/tests

# Test source files
TEST_SOURCES = test_buffer.c test_memory.c test_serialization_roundtrip.c test_reactive_manifest.c
TEST_BINARIES = $(patsubst %.c,$(TEST_BIN_DIR)/%,$(TEST_SOURCES))

# Default target: build all tests
all: $(TEST_BIN_DIR) $(TEST_BINARIES)

# Create test binary directory
$(TEST_BIN_DIR):
	mkdir -p $(TEST_BIN_DIR)

# Build individual test binaries
$(TEST_BIN_DIR)/test_%: test_%.c test_framework.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(HARFBUZZ_LIBS)

# Run all tests
test: all
	@echo "Running all unit tests..."
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $$test || exit 1; \
	done
	@echo ""
	@echo "All test suites passed!"

# Run tests with AddressSanitizer
test-asan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -g -I.. -DDEBUG -fsanitize=address -fno-omit-frame-pointer" LDFLAGS="-L../../build -lkryon_ir -lm -fsanitize=address"
	@echo "Running tests with AddressSanitizer..."
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $$test || exit 1; \
	done
	@echo ""
	@echo "All sanitized tests passed!"

# Run tests with UndefinedBehaviorSanitizer
test-ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -g -I.. -DDEBUG -fsanitize=undefined -fno-omit-frame-pointer" LDFLAGS="-L../../build -lkryon_ir -lm -fsanitize=undefined"
	@echo "Running tests with UndefinedBehaviorSanitizer..."
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $$test || exit 1; \
	done
	@echo ""
	@echo "All sanitized tests passed!"

# Run tests with both ASan and UBSan
test-asan-ubsan:
	$(MAKE) clean
	$(MAKE) all CFLAGS="-std=c99 -Wall -Wextra -g -I.. -DDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer" LDFLAGS="-L../../build -lkryon_ir -lm -fsanitize=address,undefined"
	@echo "Running tests with ASan + UBSan..."
	@for test in $(TEST_BINARIES); do \
		echo ""; \
		LD_LIBRARY_PATH=$(BUILD_DIR):$$LD_LIBRARY_PATH $$test || exit 1; \
	done
	@echo ""
	@echo "All sanitized tests passed!"

# Clean test binaries
clean:
	rm -rf $(TEST_BIN_DIR)

# Help
help:
	@echo "Kryon IR Test Suite Build Targets:"
	@echo "  all            - Build all test binaries"
	@echo "  test           - Build and run all tests"
	@echo "  test-asan      - Run tests with AddressSanitizer"
	@echo "  test-ubsan     - Run tests with UndefinedBehaviorSanitizer"
	@echo "  test-asan-ubsan - Run tests with both ASan and UBSan"
	@echo "  clean          - Remove test binaries"
	@echo "  help           - Show this help"

.PHONY: all test test-asan test-ubsan test-asan-ubsan clean help
