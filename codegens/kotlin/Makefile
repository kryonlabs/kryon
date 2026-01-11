CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC
CJSON_DIR = ../../third_party/cJSON

# Source files
SOURCES = kotlin_codegen.c
OBJECTS = kotlin_codegen.o

# cJSON dependency
CJSON_OBJ = $(CJSON_DIR)/cJSON.o

# Target library
TARGET = libkotlin_codegen.a

all: $(TARGET)

kotlin_codegen.o: kotlin_codegen.c kotlin_codegen.h
	$(CC) $(CFLAGS) -I$(CJSON_DIR) -I../../ir -c kotlin_codegen.c -o kotlin_codegen.o

# Build cJSON if needed
$(CJSON_OBJ): $(CJSON_DIR)/cJSON.c
	mkdir -p ../../build
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(CJSON_OBJ)
	ar rcs $(TARGET) $(OBJECTS) $(CJSON_OBJ)

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
