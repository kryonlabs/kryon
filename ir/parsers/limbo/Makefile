# Makefile for Limbo Parser

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -fPIC -O2
LDFLAGS = -shared

# Paths
TOP_DIR = ../../..
CODEGEN_DIR = $(TOP_DIR)/codegens
IR_DIR = $(TOP_DIR)/ir
THIRD_PARTY_DIR = $(TOP_DIR)/third_party

# Include paths
INCLUDES = \
	-I$(IR_DIR)/include \
	-I$(CODEGEN_DIR) \
	-I$(THIRD_PARTY_DIR)/cJSON \
	-I$(IR_DIR)/parsers/limbo \
	-I$(IR_DIR)/common

# Source files
SOURCES = \
	limbo_lexer.c \
	limbo_parser.c \
	limbo_ast.c \
	limbo_to_ir.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Library output
LIB = liblimboparser.so
STATIC_LIB = liblimboparser.a

# Default target
all: $(STATIC_LIB)

# Build static library
$(STATIC_LIB): $(OBJECTS)
	ar rcs $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(LIB) $(STATIC_LIB)

# Install
install: $(STATIC_LIB)
	@mkdir -p $(TOP_DIR)/build
	@cp $(STATIC_LIB) $(TOP_DIR)/build/liblimboparser.a
	@echo "Limbo parser library installed: $(TOP_DIR)/build/liblimboparser.a"

.PHONY: all clean install
