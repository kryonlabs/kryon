# DIS Codegen Makefile

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2 -Iinclude -I../../ir/include
LDFLAGS =

# Source files
SRCS = src/dis_codegen.c \
       src/module_builder.c \
       src/instruction_emitter.c

# Object files
OBJS = $(SRCS:.c=.o)

# Output library
LIB = libdiscodegen.a

# Main targets
.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(LIB)

# Dependencies
src/dis_codegen.o: src/dis_codegen.c include/dis_codegen.h src/module_builder.h
src/module_builder.o: src/module_builder.c src/module_builder.h include/dis_codegen.h
src/instruction_emitter.o: src/instruction_emitter.c src/module_builder.h
