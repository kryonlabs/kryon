# UI Examples Makefile

CC ?= cc
CFLAGS_BASE ?= -Wall -Wextra -O2
CFLAGS = $(CFLAGS_BASE) -I../include
LDFLAGS = -L.. -lflint
UNAME_S := $(shell uname -s)
RAYLIB_PKG_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
RAYLIB_PKG_LIBS := $(shell pkg-config --libs raylib 2>/dev/null || printf '%s' '-L/usr/local/lib -lraylib')
GL_X11_PKG_LIBS := $(shell pkg-config --libs gl x11 2>/dev/null || printf '%s' '-L/usr/local/lib -lGL -lX11')
GTK_PKG_CFLAGS := $(shell pkg-config --cflags gtk+-3.0 2>/dev/null)
GTK_PKG_LIBS := $(shell pkg-config --libs gtk+-3.0 2>/dev/null)
EXAMPLE_SYSTEM_LDLIBS := $(RAYLIB_PKG_LIBS) $(GL_X11_PKG_LIBS) $(GTK_PKG_LIBS) -lm -lpthread
ifeq ($(UNAME_S),Linux)
    EXAMPLE_SYSTEM_LDLIBS += -ldl -lrt
endif
FLINT_DIR := ..
FONT_OUTPUT := assets/fonts/locales
FONT_OUTPUTS := $(FONT_OUTPUT).png $(FONT_OUTPUT).dat
FONT_LOCALES := $(wildcard locales/*.txt)
FONT_TOOL := ../tools/otfchop/otfchop
FONT_SOURCE := ../tools/otfchop/unifont-17.0.04.otf
BUILD_DIR ?= ../build/examples
WEB_EMBED_ASSETS_C := $(BUILD_DIR)/embedded_asset_data.c
WEB_EMBED_ASSET_FILES := $(shell find ../themes -type f 2>/dev/null)
WEB_RAYLIB_BUILD_DIR := $(BUILD_DIR)/raylib-web
WEB_RAYLIB_A := $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_CC ?= emcc
WEB_AR ?= emar
WEB_SHELL := web_shell.html
EXAMPLES_WEB_SITE_DIR ?= ../build/site/examples
FLINT_WEB_SRCS := $(filter-out ../src/file_dialog.c,$(wildcard ../src/*.c) $(wildcard ../src/ui/*.c)) $(WEB_EMBED_ASSETS_C)
FLINT_ALL_WEB_SRCS := $(wildcard ../src/*.c) $(wildcard ../src/ui/*.c) $(WEB_EMBED_ASSETS_C)
include ../mk/raylib.mk
WEB_RAYLIB_OBJS = $(FLINT_RAYLIB_WEB_OBJS)
WEB_CFLAGS := -Wall -Wextra -std=gnu99 -O0 -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 -DUI_EMBEDDED_ONLY=1
WEB_LDFLAGS := -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sASYNCIFY -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=8388608 -sGLOBAL_BASE=16777216
WEB_OUTPUTS = $(addprefix $(EXAMPLES_WEB_SITE_DIR)/,$(addsuffix /index.html,$(EXAMPLES)))

# Raylib - use system library from nix-shell
# Check if we're in nix-shell and use its flags
CFLAGS += $(RAYLIB_PKG_CFLAGS)
CFLAGS += $(GTK_PKG_CFLAGS)
ifneq ($(NIX_CFLAGS_COMPILE),)
    CFLAGS += $(NIX_CFLAGS_COMPILE)
endif
ifneq ($(RAYLIB_DIR),)
    CFLAGS += -I$(RAYLIB_DIR)
    CFLAGS += -I$(RAYLIB_DIR)/raylib
    CFLAGS += -I$(RAYLIB_DIR)/raylib/include
endif

LDFLAGS += $(EXAMPLE_SYSTEM_LDLIBS)

EXAMPLES = 01_file_dialog 02_buttons 03_theme 04_modal 05_color 06_scaling 07_layout

.PHONY: all clean run list font-assets web web-clean

all: ../libflint.a $(EXAMPLES)

../libflint.a:
	@$(MAKE) -C ..

$(FONT_TOOL):
	@$(MAKE) -C ../tools/otfchop otfchop

font-assets: $(FONT_OUTPUTS)

$(FONT_OUTPUTS): $(FONT_LOCALES) $(FONT_TOOL) $(FONT_SOURCE)
	@mkdir -p assets/fonts
	@$(FONT_TOOL) $(FONT_SOURCE) $(FONT_LOCALES) $(FONT_OUTPUT)

%: %.c $(FONT_OUTPUTS)
	@echo "Building $@..."
	@$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "Built: $@"

web: $(WEB_OUTPUTS)

$(WEB_RAYLIB_BUILD_DIR):
	@mkdir -p $@

$(WEB_RAYLIB_BUILD_DIR)/%.o: $(RAYLIB_DIR)/%.c | $(WEB_RAYLIB_BUILD_DIR)
	@if [ -z "$(RAYLIB_DIR)" ]; then \
		echo "RAYLIB_DIR must point to raylib/src for web builds"; \
		exit 1; \
	fi
	$(WEB_CC) -c $< -o $@ \
		-Wall -D_GNU_SOURCE -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 \
		-Wno-missing-braces -Werror=pointer-arith -fno-strict-aliasing \
		-std=gnu99 -D_DEFAULT_SOURCE -Os -ffunction-sections -fdata-sections \
		-I$(RAYLIB_DIR)

$(WEB_RAYLIB_A): flint-raylib-check $(RAYLIB_SOURCES) $(WEB_RAYLIB_OBJS)
	$(WEB_AR) rcs $@ $(WEB_RAYLIB_OBJS)

$(WEB_EMBED_ASSETS_C): $(WEB_EMBED_ASSET_FILES) ../scripts/embed-assets.sh ../include/embedded_assets.h | $(BUILD_DIR)
	sh ../scripts/embed-assets.sh $@ ../themes

$(EXAMPLES_WEB_SITE_DIR)/%/index.html: %.c $(FONT_OUTPUTS) $(WEB_RAYLIB_A) $(WEB_EMBED_ASSETS_C) $(WEB_SHELL)
	@echo "Building web example $*..."
	@mkdir -p $(dir $@)
	$(WEB_CC) $(WEB_CFLAGS) \
		-I../include -I$(RAYLIB_DIR) \
		-o $(dir $@)/index.js \
		$< \
		$(if $(filter 01_file_dialog,$*),$(FLINT_ALL_WEB_SRCS),$(FLINT_WEB_SRCS)) \
		$(WEB_RAYLIB_A) \
		$(WEB_LDFLAGS) \
		--preload-file assets/fonts@assets/fonts
	perl -0pe 's#\{\{\{ SCRIPT \}\}\}#<script async src="index.js"></script>#g' $(WEB_SHELL) > $@

web-clean:
	rm -rf $(BUILD_DIR) $(EXAMPLES_WEB_SITE_DIR)

clean:
	rm -f $(EXAMPLES)
	rm -rf assets/fonts
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
