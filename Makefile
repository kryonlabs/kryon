CC ?= gcc
AR ?= ar
RAYLIB_DIR ?= ../inbe/vendor/raylib/src
BUILD_DIR ?= build
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS += -Iinclude -I$(RAYLIB_DIR)
ARFLAGS ?= rcs

SRCS = \
	src/flint_color.c \
	src/flint_scaling.c \
	src/flint_layout.c \
	src/flint_icons.c \
	src/flint_ui.c \
	src/flint_text_layout.c \
	src/flint_theme.c \
	src/flint_theme_meta.c

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
LIB = libflint.a

.PHONY: all clean

all: $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(LIB)
