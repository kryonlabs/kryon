CC ?= gcc
AR ?= ar
BUILD_DIR ?= build
SITE_DIR ?= docs/site
SITE_BUILD_DIR ?= $(BUILD_DIR)/site
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
FLINT_FONT_OUT ?= assets/fonts/locales
FLINT_FONT_LOCALES ?= locales/*.txt
FLINT_FONT_FALLBACK_LOCALES = tools/otfchop/locales/en.txt
FLINT_FONT_SOURCE ?= tools/otfchop/unifont-17.0.04.otf
FLINT_OTFCHOP ?= tools/otfchop/otfchop

# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CPPFLAGS_BASE += $(NIX_CFLAGS_COMPILE)
endif

CPPFLAGS += $(CPPFLAGS_BASE)
ARFLAGS ?= rcs

SRCS = \
	src/flint_color.c \
	src/flint_clip.c \
	src/flint_scaling.c \
	src/flint_dpi.c \
	src/flint_embedded_assets.c \
	src/flint_layout.c \
	src/flint_icons.c \
	src/flint_icon_assets.c \
	src/flint_icon_names.c \
	src/flint_text.c \
	src/flint_ui.c \
	src/flint_text_layout.c \
	src/flint_locale.c \
	src/flint_lyra_account.c \
	src/flint_theme.c \
	src/flint_theme_meta.c \
	src/flint_file_dialog.c \
	src/flint_runtime_assets.c \
	src/flint_transition.c \
	src/ui/bottom_nav.c \
	src/ui/dropdown.c \
	src/ui/guide.c \
	src/ui/icon_controls.c \
	src/ui/modal.c \
	src/ui/rows.c \
	src/ui/scroll.c \
	src/ui/tab_bar.c \
	src/ui/theme_picker.c \
	src/ui/toolbar.c \
	src/ui/tutorial.c

SRCS += $(EMBED_ASSETS_C)

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
LIB = libflint.a
CLI = $(BUILD_DIR)/flint
LYRA_ACCOUNT_TEST = $(BUILD_DIR)/tests/lyra_account_test

.PHONY: all clean run install uninstall cli font-assets docs-site test

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
	$(MAKE) -C tools/otfchop clean
	$(MAKE) -C examples web-clean

docs-site:
	rm -rf $(SITE_BUILD_DIR)
	mkdir -p $(SITE_BUILD_DIR)
	cp -R $(SITE_DIR)/. $(SITE_BUILD_DIR)/
	$(MAKE) -C examples web EXAMPLES_WEB_SITE_DIR="$(abspath $(SITE_BUILD_DIR))/examples"

test: $(LYRA_ACCOUNT_TEST)
	$(LYRA_ACCOUNT_TEST)

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

$(LYRA_ACCOUNT_TEST): tests/lyra_account_test.c src/flint_lyra_account.c include/flint_lyra_account.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/lyra_account_test.c src/flint_lyra_account.c -o $@

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/flint_icons.h
	sh scripts/embed-icons.sh $(ICON_DIR) $@

src/flint_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) scripts/embed-assets.sh include/flint_embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(FLINT_OTFCHOP): tools/otfchop/otfchop.c tools/otfchop/stb_truetype.h tools/otfchop/stb_image_write.h
	$(MAKE) -C tools/otfchop otfchop

font-assets: $(FLINT_OTFCHOP)
	@mkdir -p $(dir $(FLINT_FONT_OUT))
	@set -- $(FLINT_FONT_LOCALES); \
	case "$(FLINT_FONT_LOCALES)" in \
		*\**|*\?*) \
			if [ "$$1" = "$(FLINT_FONT_LOCALES)" ]; then \
				set -- $(FLINT_FONT_FALLBACK_LOCALES); \
			fi; \
			;; \
	esac; \
	$(FLINT_OTFCHOP) $(FLINT_FONT_SOURCE) "$$@" $(FLINT_FONT_OUT)
