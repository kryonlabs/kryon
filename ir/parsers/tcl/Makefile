# Makefile for Tcl Parser

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
	-I$(IR_DIR)/parsers/tcl

# Source files
SOURCES = \
	tcl_lexer.c \
	tcl_parser.c \
	tcl_widget_registry.c \
	tcl_to_ir.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Library output
LIB = libtclparser.so
STATIC_LIB = libtclparser.a

# Test executables
TEST = simple_test
DEBUG_TEST = debug_test

# Default target
all: $(LIB) $(STATIC_LIB)

# Test targets
test: $(TEST)
	@echo "Running simple test..."
	@./$(TEST)

debug: $(DEBUG_TEST)
	@echo "Running debug test..."
	@./$(DEBUG_TEST)

# Build shared library
$(LIB): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ -lc

# Build static library
$(STATIC_LIB): $(OBJECTS)
	ar rcs $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(LIB) $(STATIC_LIB) $(TEST) $(DEBUG_TEST) test_parser.o

# Build test executables
$(TEST): simple_test.c $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< \
		tcl_parser.o tcl_lexer.o tcl_widget_registry.o tcl_to_ir.o \
		$(THIRD_PARTY_DIR)/cJSON/cJSON.o \
		$(CODEGEN_DIR)/codegen_common.o -lm

$(DEBUG_TEST): debug_test.c $(OBJECTS)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< \
		tcl_parser.o tcl_lexer.o tcl_widget_registry.o tcl_to_ir.o \
		$(THIRD_PARTY_DIR)/cJSON/cJSON.o \
		$(CODEGEN_DIR)/codegen_common.o -lm

# Install
install: $(LIB) $(STATIC_LIB)
	@mkdir -p $(TOP_DIR)/build
	@mkdir -p $(TOP_DIR)/build/lib
	@cp $(LIB) $(STATIC_LIB) $(TOP_DIR)/build/lib/

.PHONY: all test clean install
