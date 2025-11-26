# Makefile for Kryon TypeScript bindings

CC = gcc
CFLAGS = -fPIC -shared -O2 -Wall
INCLUDES = -I../../ir -I../../backends/desktop
LIBS = -L../../build -lkryon_desktop

BUILD_DIR = build
WRAPPER_SRC = wrapper.c
WRAPPER_LIB = $(BUILD_DIR)/libkryon_ts_wrapper.so

.PHONY: all clean wrapper

all: wrapper

wrapper: $(WRAPPER_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(WRAPPER_LIB): $(WRAPPER_SRC) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $< $(LIBS)

clean:
	rm -rf $(BUILD_DIR)

# Install bun dependencies
install:
	bun install

# Type check
typecheck:
	bun run typecheck

# Run hello world example
run-hello:
	bun run ../../examples/typescript/hello_world.tsx

.PHONY: run-hello typecheck install
