CC ?= gcc
AR ?= ar
BUILD_DIR ?= build
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude
ICON_DIR ?= icons
ICON_FILES = $(wildcard $(ICON_DIR)/*.png)
ICON_ASSETS_C = src/flint_icon_assets.c

# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CPPFLAGS_BASE += $(NIX_CFLAGS_COMPILE)
endif

CPPFLAGS += $(CPPFLAGS_BASE)
ARFLAGS ?= rcs

SRCS = \
	src/flint_color.c \
	src/flint_scaling.c \
	src/flint_dpi.c \
	src/flint_layout.c \
	src/flint_icons.c \
	src/flint_icon_assets.c \
	src/flint_ui.c \
	src/flint_text_layout.c \
	src/flint_locale.c \
	src/flint_theme.c \
	src/flint_theme_meta.c \
	src/flint_file_dialog.c

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))
LIB = libflint.a

.PHONY: all clean run

all: $(LIB)

run:
	@echo "Building Flint library..."
	@$(MAKE) -C . $(LIB)
	@echo ""
	@echo "Launching examples system..."
	@$(MAKE) -C examples run

clean:
	rm -rf $(BUILD_DIR) $(LIB)

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@
