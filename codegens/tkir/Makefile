# Makefile for TKIR (Toolkit Intermediate Representation)
# Builds libkryon_tkir.a

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
CFLAGS += -std=c11 -D_POSIX_C_SOURCE=200809L
CFLAGS += -I../../include -I../../ir/include -I../../third_party/cJSON -I..

# Object files
OBJS = tkir.o tkir_builder.o tkir_emitter.o

# Library output
LIB = libkryon_tkir.a

# Default target
all: $(LIB)

# Build library
$(LIB): $(OBJS)
	ar ruvs $@ $^
	ranlib $@

# Compile object files
tkir.o: tkir.c tkir.h
	$(CC) $(CFLAGS) -c tkir.c -o tkir.o

tkir_builder.o: tkir_builder.c tkir_builder.h tkir.h
	$(CC) $(CFLAGS) -c tkir_builder.c -o tkir_builder.o

tkir_emitter.o: tkir_emitter.c tkir_emitter.h tkir.h
	$(CC) $(CFLAGS) -c tkir_emitter.c -o tkir_emitter.o

# Clean build artifacts
clean:
	rm -f $(OBJS) $(LIB)

# Install library
install: $(LIB)
	@mkdir -p ../../build
	@cp $(LIB) ../../build/
	@echo "TKIR library built: $(LIB)"

.PHONY: all clean install
