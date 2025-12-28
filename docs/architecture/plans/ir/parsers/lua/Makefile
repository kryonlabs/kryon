# Lua Parser Makefile

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -I../../.. -I../../../third_party/cJSON
LUA_CFLAGS = $(shell pkg-config --cflags lua5.4 2>/dev/null || pkg-config --cflags lua 2>/dev/null || echo "-I/usr/include/lua5.4")
LUA_LIBS = $(shell pkg-config --libs lua5.4 2>/dev/null || pkg-config --libs lua 2>/dev/null || echo "-llua5.4")

# Sources
SOURCES = ir_lua_parser.c
OBJECTS = ir_lua_parser.o
HEADERS = ir_lua_parser.h

# Default target
all: $(OBJECTS)

# Compile object file
ir_lua_parser.o: ir_lua_parser.c ir_lua_parser.h
	$(CC) $(CFLAGS) $(LUA_CFLAGS) -c ir_lua_parser.c -o ir_lua_parser.o

# Clean
clean:
	rm -f *.o

# Test
test: all
	@echo "Lua parser compiled successfully"

.PHONY: all clean test
