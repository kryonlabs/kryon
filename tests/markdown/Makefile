# Markdown Parser Tests Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -I../../ir -I../../backends/desktop
LDFLAGS = -L../../build -lkryon_ir -lm

# Test executables
TESTS = test_parser

all: $(TESTS)

test_parser: test_parser.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# Run tests
test: all
	@echo "Running markdown parser tests..."
	@LD_LIBRARY_PATH=../../build:$$LD_LIBRARY_PATH ./test_parser

# Run tests with valgrind for memory leak detection
valgrind: all
	@echo "Running tests with valgrind..."
	@LD_LIBRARY_PATH=../../build:$$LD_LIBRARY_PATH valgrind --leak-check=full --show-leak-kinds=all ./test_parser

clean:
	rm -f $(TESTS) *.o

.PHONY: all test valgrind clean
