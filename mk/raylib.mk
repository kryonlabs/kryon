FLINT_DIR ?= vendor/flint
RAYLIB_DIR ?= $(FLINT_DIR)/vendor/raylib/src

RAYLIB_SOURCES = $(shell if [ -d "$(RAYLIB_DIR)" ]; then find "$(RAYLIB_DIR)" -type f \( -name '*.c' -o -name '*.h' \); fi)

FLINT_RAYLIB_WEB_BASE_SRCS = rcore.c rshapes.c rtextures.c rtext.c raudio.c
FLINT_RAYLIB_WEB_UTILITY_SRCS = $(notdir $(wildcard $(RAYLIB_DIR)/utils.c) $(wildcard $(RAYLIB_DIR)/rutils.c))
FLINT_RAYLIB_WEB_SRCS = $(FLINT_RAYLIB_WEB_BASE_SRCS) $(FLINT_RAYLIB_WEB_UTILITY_SRCS)
FLINT_RAYLIB_WEB_OBJS = $(patsubst %.c,$(WEB_RAYLIB_BUILD_DIR)/%.o,$(FLINT_RAYLIB_WEB_SRCS))

.PHONY: flint-raylib-check

flint-raylib-check:
	@if [ ! -f "$(RAYLIB_DIR)/raylib.h" ]; then \
		echo "raylib not found at $(RAYLIB_DIR)"; \
		echo "Initialize Flint submodules or set RAYLIB_DIR=/path/to/raylib/src"; \
		exit 1; \
	fi
