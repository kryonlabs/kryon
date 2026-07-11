CC ?= cc
AR ?= ar
BUILD_DIR ?= build
SITE_DIR ?= docs/site
SITE_BUILD_DIR ?= $(BUILD_DIR)/site
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude
ICON_DIR ?= icons pfp
ICON_FILES = $(foreach dir,$(ICON_DIR),$(wildcard $(dir)/*.png))
ICON_ASSETS_C = src/ui/ui_icon_assets.c
EMBED_ASSETS ?= themes
EMBED_ASSET_FILES = $(shell find $(EMBED_ASSETS) -type f 2>/dev/null)
EMBED_ASSETS_C = $(BUILD_DIR)/embedded_asset_data.c
FLINT_COMPAT_GENERATOR = tools/generate-flint-compat.sh
FLINT_BOUNDARY_CHECK = tools/check-flint-boundaries.sh
FLINT_COMPAT_HEADER = include/flint_compat.generated.h
FLINT_BACKEND_RENAME_HEADER = $(BUILD_DIR)/generated/raylib_backend_rename.h
FLINT_RAYLIB_WRAPPERS_C = $(BUILD_DIR)/generated/flint_raylib_wrappers.c
FLINT_FONT_OUT ?= assets/fonts/locales
FLINT_FONT_OUTPUTS = $(FLINT_FONT_OUT).png $(FLINT_FONT_OUT).dat
FLINT_FONT_LOCALES ?= locales/*.txt
FLINT_FONT_LOCALE_FILES = $(wildcard $(FLINT_FONT_LOCALES))
FLINT_FONT_FALLBACK_LOCALES = tools/otfchop/locales/en.txt
FLINT_FONT_SOURCE ?= tools/otfchop/unifont-17.0.04.otf
FLINT_OTFCHOP ?= tools/otfchop/otfchop

# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CPPFLAGS_BASE += $(NIX_CFLAGS_COMPILE)
endif

CPPFLAGS += $(CPPFLAGS_BASE)
ARFLAGS ?= rcs
FLINT_DIR ?= .
FLINT_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
include mk/vendor.mk

CPPFLAGS += -DHAS_LIBOQS=1 $(FLINT_LIBOQS_INCLUDE) \
	-DHAS_LIBCURL=1 $(FLINT_CURL_CFLAGS)

SRCS := $(shell find src -type f -name '*.c' | LC_ALL=C sort)

SRCS += $(EMBED_ASSETS_C)

SYSTEM_THEME_PKG := $(shell if pkg-config --exists gtk+-3.0 2>/dev/null; then printf '%s' gtk+-3.0; fi)
ifneq ($(strip $(SYSTEM_THEME_PKG)),)
    CPPFLAGS += $(shell pkg-config --cflags $(SYSTEM_THEME_PKG)) -DSYSTEM_THEME_GTK
    LDLIBS += $(shell pkg-config --libs $(SYSTEM_THEME_PKG))
endif

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
LIB = libflint.a
LYRA_ACCOUNT_TEST = $(BUILD_DIR)/tests/lyra_account_test
LYRA_SYNC_TEST = $(BUILD_DIR)/tests/lyra_sync_test
TRANSITION_TEST = $(BUILD_DIR)/tests/transition_test
FILE_DIALOG_BACKEND_TEST = $(BUILD_DIR)/tests/file_dialog_backend_test

.PHONY: all clean run font-assets docs-site test bsd-check flint-compat flint-compat-check flint-boundary-check

all: $(LIB)

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

test: flint-compat-check flint-boundary-check $(LYRA_ACCOUNT_TEST) $(LYRA_SYNC_TEST) $(TRANSITION_TEST) $(FILE_DIALOG_BACKEND_TEST)
	$(LYRA_ACCOUNT_TEST)
	$(LYRA_SYNC_TEST)
	$(TRANSITION_TEST)
	$(FILE_DIALOG_BACKEND_TEST)

bsd-check:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) test

flint-compat: $(FLINT_COMPAT_HEADER) $(FLINT_BACKEND_RENAME_HEADER) $(FLINT_RAYLIB_WRAPPERS_C)

flint-compat-check: | $(BUILD_DIR)
	sh $(FLINT_COMPAT_GENERATOR) vendor/raylib/src/raylib.h \
		$(BUILD_DIR)/check/flint_compat.generated.h \
		$(BUILD_DIR)/check/raylib_backend_rename.h \
		$(BUILD_DIR)/check/flint_raylib_wrappers.c
	cmp $(FLINT_COMPAT_HEADER) $(BUILD_DIR)/check/flint_compat.generated.h

flint-boundary-check:
	sh $(FLINT_BOUNDARY_CHECK) .

$(LIB): $(FLINT_COMPAT_HEADER) $(OBJS) | $(FLINT_LIBOQS_A) $(FLINT_CURL_PROTOCOL_CHECK)
	$(AR) $(ARFLAGS) $@ $^

$(FLINT_COMPAT_HEADER) $(FLINT_BACKEND_RENAME_HEADER) $(FLINT_RAYLIB_WRAPPERS_C): $(FLINT_COMPAT_GENERATOR) vendor/raylib/src/raylib.h | $(BUILD_DIR)
	sh $(FLINT_COMPAT_GENERATOR) vendor/raylib/src/raylib.h \
		$(FLINT_COMPAT_HEADER) $(FLINT_BACKEND_RENAME_HEADER) \
		$(FLINT_RAYLIB_WRAPPERS_C)

$(LYRA_ACCOUNT_TEST): tests/lyra_account_test.c src/lyra/lyra_account.c include/lyra_account.h $(FLINT_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(FLINT_LIBOQS_INCLUDE) \
		tests/lyra_account_test.c src/lyra/lyra_account.c \
		$(FLINT_LIBOQS_A) -lm -o $@

$(LYRA_SYNC_TEST): tests/lyra_sync_test.c src/lyra/lyra_sync.c src/lyra/lyra_account.c include/lyra_sync.h include/lyra_account.h $(FLINT_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(FLINT_LIBOQS_INCLUDE) \
		tests/lyra_sync_test.c src/lyra/lyra_sync.c src/lyra/lyra_account.c \
		$(FLINT_LIBOQS_A) -lm -o $@

$(TRANSITION_TEST): tests/transition_test.c src/ui/ui_transition.c include/ui_transition.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/transition_test.c src/ui/ui_transition.c -o $@

$(FILE_DIALOG_BACKEND_TEST): tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c include/file_dialog.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c -o $@

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/ui_icons.h
	sh scripts/embed-icons.sh "$(ICON_DIR)" $@

src/ui/ui_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) $(FLINT_FONT_OUTPUTS) scripts/embed-assets.sh include/embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS) $(FLINT_FONT_OUTPUTS)

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

font-assets: $(FLINT_FONT_OUTPUTS)

$(FLINT_FONT_OUTPUTS): $(FLINT_OTFCHOP) $(FLINT_FONT_SOURCE) $(FLINT_FONT_FALLBACK_LOCALES) $(FLINT_FONT_LOCALE_FILES)
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
