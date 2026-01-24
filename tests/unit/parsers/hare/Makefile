# Makefile for Hare Parser Tests

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -g
IR_DIR = ../../../..
IR_BUILD = $(IR_DIR)/build
IR_INCLUDE = $(IR_DIR)/include
IR_PARSERS = $(IR_DIR)/parsers
IR_PARSERS_INCLUDE = $(IR_PARSERS)/include

SRCS = test_parser.c
OBJS = $(SRCS:.c=.o)
TARGET = test_parser

.PHONY: all test clean run

all: $(TARGET)

test: run

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -I$(IR_INCLUDE) -I$(IR_PARSERS) -I$(IR_PARSERS_INCLUDE) -L$(IR_BUILD) $< -lkryon_ir -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
