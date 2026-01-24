# Hare Codegen Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L

# Directories
IR_DIR = ../../ir
CJSON_DIR = ../../third_party/cJSON
CODEGEN_DIR = ..

# Source files
HARE_CODEGEN_SRC = hare_codegen.c
HARE_CODEGEN_HDR = hare_codegen.h
HARE_CODEGEN_OBJ = hare_codegen.o

# Default target
all: $(HARE_CODEGEN_OBJ)

# Compile hare_codegen object file
$(HARE_CODEGEN_OBJ): $(HARE_CODEGEN_SRC) $(HARE_CODEGEN_HDR)
	$(CC) $(CFLAGS) -I$(IR_DIR)/include -I$(CJSON_DIR) -I$(CODEGEN_DIR) -c $(HARE_CODEGEN_SRC) -o $(HARE_CODEGEN_OBJ)

# Clean
clean:
	rm -f $(HARE_CODEGEN_OBJ)

# Install headers (placeholder for compatibility)
install:
	@echo "Hare codegen headers install - nothing to do"

.PHONY: all clean install
