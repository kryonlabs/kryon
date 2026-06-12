CC ?= gcc
AR ?= ar
BUILD_DIR ?= build
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude
ICON_DIR ?= icons
ICON_FILES = $(wildcard $(ICON_DIR)/*.png)
ICON_ASSETS_C = src/flint_icon_assets.c
EMBED_ASSETS ?= themes
EMBED_ASSET_FILES = $(shell find $(EMBED_ASSETS) -type f 2>/dev/null)
EMBED_ASSETS_C = $(BUILD_DIR)/flint_embedded_asset_data.c

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
	src/flint_text.c \
	src/flint_ui.c \
	src/flint_text_layout.c \
	src/flint_locale.c \
	src/flint_theme.c \
	src/flint_theme_meta.c \
	src/flint_file_dialog.c

SRCS += $(EMBED_ASSETS_C)

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
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

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/flint_icons.h
	sh scripts/embed-icons.sh $(ICON_DIR) $@

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) scripts/embed-assets.sh include/flint_embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@
