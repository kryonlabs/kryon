# Makefile for KRY Expression Parser Tests

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../../../../include -I../../../../ir/include -I../../../../ir/parsers/kry
LDFLAGS = -L../../../../build -lkryon_ir -lm

# Source files
TEST_SRC = test_hare_templates.c
TEST_BIN = test_hare_templates

# KRY expression parser object file
KRY_EXPR_OBJ = ../../../../ir/parsers/kry/kry_expression.o

# Default target
all: $(TEST_BIN)

# Build test binary
$(TEST_BIN): $(TEST_SRC) $(KRY_EXPR_OBJ) | ../../../../build/libkryon_ir.a
	$(MAKE) -C ../../../../ir static
	$(CC) $(CFLAGS) $(TEST_SRC) $(KRY_EXPR_OBJ) -o $(TEST_BIN) $(LDFLAGS)

# Build IR library if needed
../../../../build/libkryon_ir.a:
	$(MAKE) -C ../../../../ir static

# Build KRY expression parser if needed
$(KRY_EXPR_OBJ):
	$(MAKE) -C ../../../../ir/parsers/kry

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean
clean:
	rm -f $(TEST_BIN)

.PHONY: all test clean
