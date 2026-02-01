# Lua Codegen Makefile
# ===================

# Load shared codegen template
include ../common.mk

# Lua-specific settings (note: lua_codegen_adapter.c is built in parent Makefile)
SOURCES := lua_codegen.c
HEADERS := lua_codegen.h
CFLAGS += -D_POSIX_C_SOURCE=200809L
