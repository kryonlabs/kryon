# Makefile for Kryon C Frontend Bindings

CC = gcc
AR = ar
CFLAGS = -Wall -Wextra -std=c11 -O2 -fPIC
INCLUDES = -I../../ir -I../../backends/desktop -I../../third_party/cJSON

SOURCES = kryon.c
OBJECTS = $(SOURCES:.c=.o)
LIBRARY = libkryon_c.a

# Build paths
BUILD_DIR = ../../build

.PHONY: all clean install

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $^
	@echo "Built $(LIBRARY)"

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(LIBRARY)

install: $(LIBRARY)
	mkdir -p $(BUILD_DIR)
	cp $(LIBRARY) $(BUILD_DIR)/
	@echo "Installed $(LIBRARY) to $(BUILD_DIR)/"

# Development targets
.PHONY: check format

check:
	$(CC) $(CFLAGS) $(INCLUDES) -fsyntax-only $(SOURCES)

format:
	clang-format -i *.c *.h
