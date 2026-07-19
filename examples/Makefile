# UI Examples Makefile

CC ?= cc
CFLAGS_BASE ?= -Wall -Wextra -O2
CFLAGS = $(CFLAGS_BASE) -I../include
LDFLAGS = -L.. -lflint
UNAME_S := $(shell uname -s)
GL_X11_PKG_LIBS := $(shell pkg-config --libs gl x11 2>/dev/null || printf '%s' '-L/usr/local/lib -lGL -lX11')
GTK_PKG_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_PKG_LIBS := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
RAY_PKGS ?= sdl2 libdrm gbm egl glesv2
RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_CFLAGS ?= $(strip $(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS))
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
EXAMPLE_SYSTEM_LDLIBS := $(RAY_LDLIBS) $(GL_X11_PKG_LIBS) $(GTK_PKG_LIBS) -lm -lpthread
ifeq ($(UNAME_S),Linux)
    EXAMPLE_SYSTEM_LDLIBS += -ldl -lrt
endif
FLINT_DIR := ..
FONT_OUTPUTS := $(wildcard ../fonts/noto/*.ttf ../fonts/noto/*.otf)
BUILD_DIR ?= ../build/examples
RAYLIB_BUILD_DIR := $(BUILD_DIR)/raylib-native
RAYLIB_LIB_NAME := raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/lib$(RAYLIB_LIB_NAME).a
WEB_EMBED_ASSETS_C := $(BUILD_DIR)/embedded_asset_data.c
WEB_EMBED_ASSET_FILES := $(shell find ../themes -type f 2>/dev/null)
WEB_RAYLIB_BUILD_DIR := $(BUILD_DIR)/raylib-web
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
FLINT_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
FLINT_WEB_LIBOQS_BUILD_DIR ?= $(FLINT_VENDOR_BUILD_DIR)/web-liboqs
include ../mk/vendor.mk
WEB_CC ?= emcc
WEB_AR ?= emar
WEB_SHELL := web_shell.html
EXAMPLES_WEB_SITE_DIR ?= ../build/site/examples
include ../mk/raylib.mk
FLINT_LIBRARY_SRCS := $(shell find ../src -type f -name '*.c' | LC_ALL=C sort)
FLINT_WEB_SRCS := $(filter-out ../src/file_dialog/file_dialog.c,$(FLINT_LIBRARY_SRCS)) $(WEB_EMBED_ASSETS_C) $(FLINT_RAYLIB_WRAPPERS_C)
FLINT_ALL_WEB_SRCS := $(FLINT_LIBRARY_SRCS) $(WEB_EMBED_ASSETS_C) $(FLINT_RAYLIB_WRAPPERS_C)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -O0 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 -DUI_EMBEDDED_ONLY=1 -DHAS_LIBOQS=1 -I$(FLINT_WEB_LIBOQS_BUILD_DIR)/include
WEB_LDFLAGS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=8388608 -sGLOBAL_BASE=16777216
WEB_OUTPUTS = $(addprefix $(EXAMPLES_WEB_SITE_DIR)/,$(addsuffix /index.html,$(EXAMPLES)))

# Check if we're in nix-shell and use its flags
CFLAGS += $(GTK_PKG_CFLAGS)
ifneq ($(NIX_CFLAGS_COMPILE),)
    CFLAGS += $(NIX_CFLAGS_COMPILE)
endif

LDFLAGS += $(RAYLIB_A) $(EXAMPLE_SYSTEM_LDLIBS)

EXAMPLES = 01_file_dialog 02_buttons 03_theme 04_modal 05_color 06_scaling 07_layout 08_editor_gallery \
	09_geometry 10_menus 11_basic_controls 12_collections 13_text_editor \
	14_canvas 15_containers 16_dialogs 17_keyboard_platform 18_accessibility

.PHONY: all clean run list font-assets web web-clean

all: ../libflint.a $(EXAMPLES)

../libflint.a:
	@$(MAKE) -C ..

font-assets:
	@printf '%s\n' 'font assets are checked in'

%: %.c ../libflint.a $(RAYLIB_A) $(FONT_OUTPUTS)
	@echo "Building $@..."
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

web: $(WEB_OUTPUTS)

$(BUILD_DIR):
	@mkdir -p $@

$(WEB_EMBED_ASSETS_C): $(WEB_EMBED_ASSET_FILES) ../scripts/embed-assets.sh ../include/embedded_assets.h | $(BUILD_DIR)
	sh ../scripts/embed-assets.sh $@ ../themes

$(EXAMPLES_WEB_SITE_DIR)/%/index.html: %.c $(FONT_OUTPUTS) $(WEB_RAYLIB_A) $(FLINT_WEB_LIBOQS_A) $(WEB_EMBED_ASSETS_C) $(WEB_SHELL)
	@echo "Building web example $*..."
	@mkdir -p $(dir $@)
	$(WEB_CC) $(WEB_CFLAGS) \
		-I../include \
		-o $(dir $@)/index.js \
		$< \
		$(if $(filter 01_file_dialog,$*),$(FLINT_ALL_WEB_SRCS),$(FLINT_WEB_SRCS)) \
		$(WEB_RAYLIB_A) \
		$(FLINT_WEB_LIBOQS_A) \
		$(WEB_LDFLAGS) \
		--preload-file ../fonts/noto@fonts/noto
	perl -0pe 's#\{\{\{ SCRIPT \}\}\}#<script async src="index.js"></script>#g' $(WEB_SHELL) > $@

web-clean:
	rm -rf $(BUILD_DIR) $(EXAMPLES_WEB_SITE_DIR)

clean:
	rm -f $(EXAMPLES)
	rm -rf $(BUILD_DIR)

list:
	@echo "UI Examples:"
	@for i in $(EXAMPLES); do \
		echo "  $$i"; \
		done

run:
	@echo "UI Examples"
	@echo "================"
	@echo ""
	@echo "Available examples:"
	@echo "  1) file_dialog"
	@echo "  2) buttons"
	@echo "  3) theme"
	@echo "  4) modal"
	@echo "  5) color"
	@echo "  6) scaling"
	@echo "  7) layout"
	@echo "  8) editor_gallery"
	@echo "  9) geometry"
	@echo "  10) menus"
	@echo "  11) basic_controls"
	@echo "  12) collections"
	@echo "  13) text_editor"
	@echo "  14) canvas"
	@echo "  15) containers"
	@echo "  16) dialogs"
	@echo "  17) keyboard_platform"
	@echo "  18) accessibility"
	@echo ""
	@read -p "Enter example number: " num; \
	case "$$num" in \
		1) example="01_file_dialog";; \
		2) example="02_buttons";; \
		3) example="03_theme";; \
		4) example="04_modal";; \
		5) example="05_color";; \
		6) example="06_scaling";; \
		7) example="07_layout";; \
		8) example="08_editor_gallery";; \
		9) example="09_geometry";; \
		10) example="10_menus";; \
		11) example="11_basic_controls";; \
		12) example="12_collections";; \
		13) example="13_text_editor";; \
		14) example="14_canvas";; \
		15) example="15_containers";; \
		16) example="16_dialogs";; \
		17) example="17_keyboard_platform";; \
		18) example="18_accessibility";; \
		*) example=;; \
	esac; \
	if [ -n "$$example" ]; then \
		$(MAKE) $$example; \
		if [ -f "$$example" ]; then \
			echo ""; \
			echo "Running $$example..."; \
			./$$example; \
		else \
			echo "Failed to build $$example"; \
		fi; \
	else \
		echo "Invalid example number"; \
	fi
