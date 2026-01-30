# Makefile for Kryon C Frontend Bindings

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c11 -O2 -fPIC
INCLUDES = -I../../include -I../../ir/include -I../../runtime/desktop -I../../third_party/cJSON \
           -I../../third_party/tomlc99 -I../../ir/src/utils -I../../core/include

SOURCES = kryon.c kryon_dsl.c kryon_reactive.c kryon_reactive_ui.c kryon_plugins.c
OBJECTS = $(SOURCES:.c=.o)
LIBRARY = libkryon_c.a

# DSL Runtime for desktop target
DSL_RUNTIME_SRC = kryon_dsl_runtime.c
DSL_RUNTIME_OBJ = kryon_dsl_runtime.o
DSL_RUNTIME_LIB = libkryon_dsl_runtime.a

# Build paths
BUILD_DIR = ../../build

.PHONY: all clean install dsl_runtime

all: $(LIBRARY) dsl_runtime

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $^
	@echo "Built $(LIBRARY)"

dsl_runtime: $(DSL_RUNTIME_LIB)

$(DSL_RUNTIME_LIB): $(DSL_RUNTIME_OBJ)
	$(AR) rcs $@ $^
	@echo "Built $(DSL_RUNTIME_LIB)"

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY) $(DSL_RUNTIME_OBJ) $(DSL_RUNTIME_LIB)

install: $(LIBRARY) dsl_runtime
	mkdir -p $(BUILD_DIR)
	cp $(LIBRARY) $(BUILD_DIR)/
	cp $(DSL_RUNTIME_LIB) $(BUILD_DIR)/
	@echo "Installed $(LIBRARY) and $(DSL_RUNTIME_LIB) to $(BUILD_DIR)/"

# Development targets
.PHONY: check format

check:
	$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(SOURCES)

format:
	clang-format -i *.c *.h
