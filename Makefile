CC ?= gcc
AR ?= ar
BUILD_DIR ?= build
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude
INSTALL_DIR ?= $(HOME)/.local/share/flint
BIN_DIR ?= $(HOME)/bin
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
	src/flint_file_dialog.c \
	src/flint_runtime_assets.c

SRCS += $(EMBED_ASSETS_C)

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
LIB = libflint.a
CLI = $(BUILD_DIR)/flint

.PHONY: all clean run install uninstall cli

all: $(LIB)

cli: $(CLI)

run:
	@echo "Building Flint library..."
	@$(MAKE) -C . $(LIB)
	@echo ""
	@echo "Launching examples system..."
	@$(MAKE) -C examples run

clean:
	rm -rf $(BUILD_DIR) $(LIB)

install: $(CLI)
	@echo "Installing flint to $(INSTALL_DIR)..."
	@mkdir -p $(INSTALL_DIR)
	@mkdir -p $(BIN_DIR)
	@cp $(CLI) $(INSTALL_DIR)/flint.tmp
	@mv -f $(INSTALL_DIR)/flint.tmp $(INSTALL_DIR)/flint
	@if [ -L $(BIN_DIR)/flint ]; then \
		echo "Removing existing symlink: $(BIN_DIR)/flint"; \
		rm $(BIN_DIR)/flint; \
	elif [ -e $(BIN_DIR)/flint ]; then \
		echo "Error: $(BIN_DIR)/flint exists and is not a symlink"; \
		echo "Please remove it manually and try again"; \
		exit 1; \
	fi
	@ln -s $(INSTALL_DIR)/flint $(BIN_DIR)/flint
	@echo "Created symlink: $(BIN_DIR)/flint -> $(INSTALL_DIR)/flint"
	@echo "Installation complete. Run 'flint help'."

uninstall:
	@echo "Uninstalling flint..."
	@if [ -L $(BIN_DIR)/flint ]; then \
		echo "Removing symlink: $(BIN_DIR)/flint"; \
		rm $(BIN_DIR)/flint; \
	elif [ -e $(BIN_DIR)/flint ]; then \
		echo "Warning: $(BIN_DIR)/flint exists but is not a symlink"; \
		echo "Skipping symlink removal"; \
	fi
	@if [ -d $(INSTALL_DIR) ]; then \
		echo "Removing directory: $(INSTALL_DIR)"; \
		rm -rf $(INSTALL_DIR); \
	fi
	@echo "Uninstall complete"

$(LIB): $(OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(CLI): cli/flint.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

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
