# Makefile for WIR (Toolkit Intermediate Representation)
# Builds libkryon_wir.a

CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2 -g
CFLAGS += -std=c11 -D_POSIX_C_SOURCE=200809L
CFLAGS += -I../../include -I../../ir/include -I../../third_party/cJSON -I..

# Object files
OBJS = wir.o wir_builder.o wir_emitter.o

# Library output
LIB = libkryon_wir.a

# Default target
all: $(LIB)

# Build library
$(LIB): $(OBJS)
	ar ruvs $@ $^
	ranlib $@

# Compile object files
wir.o: wir.c wir.h
	$(CC) $(CFLAGS) -c wir.c -o wir.o

wir_builder.o: wir_builder.c wir_builder.h wir.h
	$(CC) $(CFLAGS) -c wir_builder.c -o wir_builder.o

wir_emitter.o: wir_emitter.c wir_emitter.h wir.h
	$(CC) $(CFLAGS) -c wir_emitter.c -o wir_emitter.o

# Clean build artifacts
clean:
	rm -f $(OBJS) $(LIB)

# Install library
install: $(LIB)
	@mkdir -p ../../build
	@cp $(LIB) ../../build/
	@echo "WIR library built: $(LIB)"

.PHONY: all clean install
