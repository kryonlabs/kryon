# DIS Bytecode Compiler Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -Isrc -I../ir/include -I../third_party/cJSON
CJSON_O = ../third_party/cJSON/cJSON.o

# Source files
SRCS = src/dis_codegen.c \
       src/module_builder.c \
       src/instruction_emitter.c \
       src/type_descriptor.c \
       src/data_section.c \
       src/link_section.c \
       src/kir_translator.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output library
LIB = libdiscodegen.a

# Main targets
.PHONY: all clean

all: $(CJSON_O) $(LIB)

$(CJSON_O):
	$(MAKE) -C ../third_party/cJSON cJSON.o

$(LIB): $(OBJS) $(CJSON_O)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)
	$(MAKE) -C ../third_party/cJSON clean

# Dependencies
src/dis_codegen.o: src/dis_codegen.c include/dis_codegen.h src/module_builder.h src/internal.h
src/module_builder.o: src/module_builder.c src/module_builder.h src/internal.h include/dis_codegen.h
src/instruction_emitter.o: src/instruction_emitter.c src/module_builder.h src/internal.h
src/type_descriptor.o: src/type_descriptor.c src/internal.h
src/data_section.o: src/data_section.c src/internal.h
src/link_section.o: src/link_section.c src/internal.h
src/kir_translator.o: src/kir_translator.c src/internal.h include/dis_codegen.h
