# UI Examples Makefile

CC ?= cc
CFLAGS_BASE ?= -Wall -Wextra -O2
CFLAGS = $(CFLAGS_BASE) -I. -I../include
LDFLAGS = -L.. -lkryon
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
KRYON_DIR := ..
FONT_OUTPUTS := $(wildcard ../fonts/noto/*.ttf ../fonts/noto/*.otf)
BUILD_DIR ?= ../build/examples
KC ?= ../build/bin/kc
CODEGEN_DIR := $(BUILD_DIR)/codegen
BIN_DIR := $(BUILD_DIR)/bin
RAYLIB_BUILD_DIR := $(BUILD_DIR)/raylib-native
RAYLIB_LIB_NAME := raylib
RAYLIB_A := $(RAYLIB_BUILD_DIR)/lib$(RAYLIB_LIB_NAME).a
WEB_EMBED_ASSETS_C := $(BUILD_DIR)/embedded_asset_data.c
WEB_EMBED_ASSET_FILES := $(shell find ../themes -type f 2>/dev/null)
WEB_RAYLIB_BUILD_DIR := $(BUILD_DIR)/raylib-web
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
KRYON_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
KRYON_WEB_LIBOQS_BUILD_DIR ?= $(KRYON_VENDOR_BUILD_DIR)/web-liboqs
include ../mk/vendor.mk
WEB_CC ?= emcc
WEB_AR ?= emar
EMSCRIPTEN_FILE_PACKAGER ?= /usr/local/lib/emscripten/tools/file_packager
WEB_SHELL := web_shell.html
EXAMPLES_WEB_SITE_DIR ?= ../build/site/examples
include ../mk/raylib.mk
KRYON_LIBRARY_SRCS := $(shell find ../src -type f -name '*.c' | LC_ALL=C sort)
KRYON_WEB_SRCS := $(filter-out ../src/file_dialog/file_dialog.c,$(KRYON_LIBRARY_SRCS)) $(WEB_EMBED_ASSETS_C) $(KRYON_RAYLIB_WRAPPERS_C)
KRYON_ALL_WEB_SRCS := $(KRYON_LIBRARY_SRCS) $(WEB_EMBED_ASSETS_C) $(KRYON_RAYLIB_WRAPPERS_C)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -O0 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 -DUI_EMBEDDED_ONLY=1 -DHAS_LIBOQS=1 -I$(KRYON_WEB_LIBOQS_BUILD_DIR)/include
WEB_LDFLAGS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=8388608 -sGLOBAL_BASE=16777216
WEB_PRELOAD_FLAGS ?= $(if $(wildcard $(EMSCRIPTEN_FILE_PACKAGER)),--preload-file ../fonts/noto@fonts/noto,)
WEB_OUTPUTS = $(addprefix $(EXAMPLES_WEB_SITE_DIR)/,$(addsuffix /index.html,$(EXAMPLES)))

# Check if we're in nix-shell and use its flags
CFLAGS += $(GTK_PKG_CFLAGS) -I$(CODEGEN_DIR)
ifneq ($(NIX_CFLAGS_COMPILE),)
    CFLAGS += $(NIX_CFLAGS_COMPILE)
endif

LDFLAGS += $(RAYLIB_A) $(EXAMPLE_SYSTEM_LDLIBS)

EXAMPLES = 01_file_dialog 02_buttons 03_theme 04_modal 05_color 06_scaling 07_layout \
	09_geometry 10_menus 11_basic_controls 12_collections 13_text_editor \
	14_canvas 15_containers 16_dialogs 17_keyboard_platform 18_accessibility
EXAMPLE_BINS := $(addprefix $(BIN_DIR)/,$(EXAMPLES))

.PHONY: all clean run list font-assets web web-clean
.SECONDARY: $(addprefix $(CODEGEN_DIR)/,$(addsuffix .c,$(EXAMPLES))) $(addprefix $(CODEGEN_DIR)/,$(addsuffix .h,$(EXAMPLES)))

all: ../libkryon.a $(KC) $(EXAMPLE_BINS)

../libkryon.a:
	@$(MAKE) -C ..

$(KC):
	@$(MAKE) -C .. build/bin/kc

font-assets:
	@printf '%s\n' 'font assets are checked in'

$(CODEGEN_DIR):
	@mkdir -p $@

$(BIN_DIR):
	@mkdir -p $@

$(CODEGEN_DIR)/%.c $(CODEGEN_DIR)/%.h: %.kry $(KC) | $(CODEGEN_DIR)
	@echo "Generating $*..."
	@$(KC) --root . -o $(CODEGEN_DIR) $<

$(BIN_DIR)/%: $(CODEGEN_DIR)/%.c ../libkryon.a $(RAYLIB_A) $(FONT_OUTPUTS) | $(BIN_DIR)
	@echo "Building $@..."
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

$(EXAMPLES): %: $(BIN_DIR)/%

web: $(WEB_OUTPUTS)

$(BUILD_DIR):
	@mkdir -p $@

$(WEB_EMBED_ASSETS_C): $(WEB_EMBED_ASSET_FILES) ../scripts/embed-assets.sh ../include/embedded_assets.h | $(BUILD_DIR)
	sh ../scripts/embed-assets.sh $@ ../themes

$(EXAMPLES_WEB_SITE_DIR)/%/index.html: $(CODEGEN_DIR)/%.c $(FONT_OUTPUTS) $(WEB_RAYLIB_A) $(KRYON_WEB_LIBOQS_A) $(WEB_EMBED_ASSETS_C) $(WEB_SHELL)
	@echo "Building web example $*..."
	@mkdir -p $(dir $@)
	$(WEB_CC) $(WEB_CFLAGS) \
		-I. -I../include -I$(CODEGEN_DIR) \
		-o $(dir $@)/index.js \
		$< \
		$(if $(filter 01_file_dialog,$*),$(KRYON_ALL_WEB_SRCS),$(KRYON_WEB_SRCS)) \
		$(WEB_RAYLIB_A) \
		$(KRYON_WEB_LIBOQS_A) \
		$(WEB_LDFLAGS) \
		$(WEB_PRELOAD_FLAGS)
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
	@echo "  8) geometry"
	@echo "  9) menus"
	@echo "  10) basic_controls"
	@echo "  11) collections"
	@echo "  12) text_editor"
	@echo "  13) canvas"
	@echo "  14) containers"
	@echo "  15) dialogs"
	@echo "  16) keyboard_platform"
	@echo "  17) accessibility"
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
		8) example="09_geometry";; \
		9) example="10_menus";; \
		10) example="11_basic_controls";; \
		11) example="12_collections";; \
		12) example="13_text_editor";; \
		13) example="14_canvas";; \
		14) example="15_containers";; \
		15) example="16_dialogs";; \
		16) example="17_keyboard_platform";; \
		17) example="18_accessibility";; \
		*) example=;; \
	esac; \
	if [ -n "$$example" ]; then \
		$(MAKE) $(BIN_DIR)/$$example; \
		if [ -f "$(BIN_DIR)/$$example" ]; then \
			echo ""; \
			echo "Running $$example..."; \
			$(BIN_DIR)/$$example; \
		else \
			echo "Failed to build $$example"; \
		fi; \
	else \
		echo "Invalid example number"; \
	fi
