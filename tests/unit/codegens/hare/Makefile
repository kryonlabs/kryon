# Makefile for Hare Codegen Tests

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -I../../../../include -I../../../../ir/include -I../../../../codegens/hare -I../../../../third_party/cJSON
LDFLAGS = -L../../../../build -lkryon_ir -lm

# Source files
TEST_SRC = test_codegen.c
TEST_BIN = test_codegen

# Hare codegen object file
HARE_CODEGEN_OBJ = ../../../../codegens/hare/hare_codegen.o

# Codegen common object file
CODEGEN_COMMON_OBJ = ../../../../build/codegen_common.o

# cJSON object file
CJSON_OBJ = ../../../../third_party/cJSON/cJSON.o

# Default target
all: $(TEST_BIN)

# Build test binary
$(TEST_BIN): $(TEST_SRC) $(HARE_CODEGEN_OBJ) $(CODEGEN_COMMON_OBJ) $(CJSON_OBJ) | ../../../../build/libkryon_ir.a
	$(MAKE) -C ../../../../ir static
	$(CC) $(CFLAGS) $(TEST_SRC) $(HARE_CODEGEN_OBJ) $(CODEGEN_COMMON_OBJ) $(CJSON_OBJ) -o $(TEST_BIN) $(LDFLAGS)

# Build IR library if needed
../../../../build/libkryon_ir.a:
	$(MAKE) -C ../../../../ir static

# Build Hare codegen if needed
$(HARE_CODEGEN_OBJ):
	$(MAKE) -C ../../../../codegens/hare

# Build cJSON if needed
$(CJSON_OBJ):
	$(CC) $(CFLAGS) -c ../../../../third_party/cJSON/cJSON.c -o $(CJSON_OBJ)

# Run tests
test: $(TEST_BIN)
	./$(TEST_BIN)

# Clean
clean:
	rm -f $(TEST_BIN) $(CJSON_OBJ)

.PHONY: all test clean
