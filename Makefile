CC ?= cc
AR ?= ar
UNAME_S := $(shell uname -s 2>/dev/null)
UNAME_M := $(shell uname -m 2>/dev/null)
ifeq ($(UNAME_M),amd64)
    KRYON_ARCH := x86_64
else
    KRYON_ARCH := $(UNAME_M)
endif
ifeq ($(UNAME_S),Linux)
    KRYON_PLATFORM := linux
else ifeq ($(UNAME_S),FreeBSD)
    KRYON_PLATFORM := freebsd
else ifeq ($(UNAME_S),Darwin)
    KRYON_PLATFORM := macos
else
    KRYON_PLATFORM := $(UNAME_S)
endif
BUILD_ROOT ?= build
BUILD_DIR ?= $(BUILD_ROOT)/$(KRYON_PLATFORM)-$(KRYON_ARCH)
PREFIX ?= $(HOME)/.local
SITE_DIR ?= docs/site
SITE_BUILD_DIR ?= $(BUILD_DIR)/site
VERSION_FILE ?= include/kryon_version.h
KRYON_VERSION_STRING := $(shell sed -n 's/^#define KRYON_VERSION_STRING "\([^"]*\)".*/\1/p' $(VERSION_FILE) 2>/dev/null)
VERSION ?= $(if $(strip $(KRYON_VERSION_STRING)),$(KRYON_VERSION_STRING),$(shell git describe --tags --always --dirty 2>/dev/null || printf '%s' 0.0.0))
DIST_DIR ?= dist
STATIC_DIST_ROOT := $(BUILD_DIR)/dist/kryon-$(VERSION)-static
STATIC_DIST_ARCHIVE := $(DIST_DIR)/kryon-$(VERSION)-static.tar.gz
TOOLS_DIST_ROOT := $(BUILD_DIR)/dist/kryon-$(VERSION)-tools-$(KRYON_PLATFORM)-$(KRYON_ARCH)
TOOLS_DIST_ARCHIVE := $(DIST_DIR)/kryon-$(VERSION)-tools-$(KRYON_PLATFORM)-$(KRYON_ARCH).tar.gz
K2C = $(BUILD_DIR)/bin/k2c
K2G = $(BUILD_DIR)/bin/k2g
K2IR = $(BUILD_DIR)/bin/k2ir
K2B = $(BUILD_DIR)/bin/k2b
KT = $(BUILD_DIR)/bin/kt
KRYON_PREVIEW = $(BUILD_DIR)/bin/kryon-preview
KRYON_CMD = $(BUILD_DIR)/bin/kryon
KRB_RUN = $(BUILD_DIR)/bin/krb-run
KRB_SDL = $(BUILD_DIR)/bin/krb-sdl
KRY_SW_TEST = $(BUILD_DIR)/tests/kry_sw_test
KRB_LOGIC_TEST = $(BUILD_DIR)/tests/krb_logic_test
KRB_ASSET_TEST = $(BUILD_DIR)/tests/krb_asset_test
KRB_CAPS_TEST = $(BUILD_DIR)/tests/krb_caps_test
EMCC ?= emcc
KRB_WEB_DIR = $(BUILD_DIR)/web/krb-web
KRB_WEB = $(KRB_WEB_DIR)/index.html
KRB_WEB_KRY ?= examples/02_buttons.kry
BINDIR ?= $(PREFIX)/bin
INSTALL ?= install
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude $(KRYON_PHYSICS_CPPFLAGS)
ICON_DIR ?= icons
ICON_FILES = $(shell find $(ICON_DIR) -path '*/review/*' -prune -o -type f -name '*.png' -print 2>/dev/null | LC_ALL=C sort)
ICON_ASSETS_C = src/ui/ui_icon_assets.c
# Default embedded assets: themes + the regular UI font. The CJK Noto faces
# (JP/KR/SC/TC, ~22 MB) are intentionally NOT embedded by default — nothing in
# the default UI loads them. Apps that need CJK can override:
#   make EMBED_ASSETS="themes fonts/noto"
EMBED_ASSETS ?= themes fonts/noto/NotoSans-Regular.ttf
EMBED_ASSET_FILES = $(shell find $(EMBED_ASSETS) -type f 2>/dev/null)
EMBED_ASSETS_C = $(BUILD_DIR)/embedded_asset_data.c
FONT_SUBSET_OUT_DIR ?= $(BUILD_DIR)/fonts/subset
FONT_SUBSET_SOURCE_DIR ?= $(KRYON_DIR)/fonts/noto
FONT_SUBSET_PREFIX ?= App
FONT_SUBSET_CORPUS ?=
KRYON_COMPAT_GENERATOR = tools/generate-kryon-compat.sh
KRYON_BOUNDARY_CHECK = tools/check-kryon-boundaries.sh
KRYON_COMPAT_SYMBOL_CHECK = tools/check-raylib-compat-symbols.sh
KRYON_COMPAT_HEADER = include/kryon_compat.generated.h
KRYON_BACKEND_RENAME_HEADER = $(BUILD_DIR)/generated/raylib_backend_rename.h
KRYON_RAYLIB_WRAPPERS_C = $(BUILD_DIR)/generated/kryon_raylib_wrappers.c
KRYON_NULL_BACKEND_C = $(BUILD_DIR)/generated/kryon_null_backend.c
KRYON_RAYLIB_GENERATED_PUBLIC_HEADER ?= $(KRYON_COMPAT_HEADER)
KRYON_RAYLIB_BACKEND_RENAME_HEADER ?= $(KRYON_BACKEND_RENAME_HEADER)
KRYON_BACKEND_STAMP = $(BUILD_DIR)/.backend-$(KRYON_BACKEND)

# Graphics/input backend. The kryon surface (kryon_compat.generated.h) is
# backend-neutral; the concrete implementation is selected at link time here.
#   raylib  -> generated raylib forwarders + libraylib.a   (default, unchanged)
#   canvas  -> src/backend/canvas_*.c (HTML5 Canvas2D; no raylib)
#   null    -> generated zero-return stubs  (no-ops; for headless tests)
KRYON_BACKEND ?= raylib
KRYON_CANVAS_SRCS := $(wildcard src/backend/canvas_*.c)
KRYON_LIBDRAW_SRCS := $(wildcard src/backend/libdraw_*.c)
ifeq ($(KRYON_BACKEND),raylib)
  KRYON_BACKEND_SRCS = $(KRYON_RAYLIB_WRAPPERS_C)
else ifeq ($(KRYON_BACKEND),canvas)
  # the canvas sources live in src/ and arrive via the SRCS find below;
  # appending them here would compile every canvas TU twice.
  KRYON_BACKEND_SRCS =
else ifeq ($(KRYON_BACKEND),libdraw)
  # the libdraw sources live in src/ and arrive via the SRCS find below;
  # appending them here would compile every libdraw TU twice.
  KRYON_BACKEND_SRCS =
else ifeq ($(KRYON_BACKEND),null)
  KRYON_BACKEND_SRCS = $(KRYON_NULL_BACKEND_C)
else
  $(error Unknown KRYON_BACKEND '$(KRYON_BACKEND)' (expected raylib, canvas, libdraw, or null))
endif

# Link inputs for the selected backend: only raylib needs libraylib.a and the
# window/GL system libraries it was built with; null and canvas carry their
# own (or none), so KRYON_BACKEND=null links headless with no raylib at all.
ifeq ($(KRYON_BACKEND),raylib)
  KRYON_BACKEND_LIBS = $(RAYLIB_A)
  KRYON_BACKEND_LDLIBS ?= $(RAY_LDLIBS)
else ifeq ($(KRYON_BACKEND),libdraw)
  PLAN9PORT_DIR ?= /mnt/storage/Projects/plan9port
  CPPFLAGS += -DKRYON_BACKEND_LIBDRAW -I$(PLAN9PORT_DIR)/include -idirafter $(RAYLIB_DIR)/external
  KRYON_BACKEND_LIBS =
  KRYON_BACKEND_LDLIBS ?= -L$(PLAN9PORT_DIR)/lib -ldraw -lmemdraw -lmux -lthread -l9 -lpthread -lm
else
  KRYON_BACKEND_LIBS =
  KRYON_BACKEND_LDLIBS ?=
endif
RAYLIB_DIR ?= $(KRYON_DIR)/vendor/raylib/src
RAYLIB_BUILD_DIR ?= $(BUILD_DIR)/raylib
RAYLIB_A ?= $(RAYLIB_BUILD_DIR)/libraylib.a
RAY_PKGS ?= sdl2 libdrm gbm egl glesv2
RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_CFLAGS ?= $(strip $(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS))
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
CURL_CODEC_LDLIBS ?= $(strip \
  $(shell pkg-config --libs libbrotlidec 2>/dev/null) \
  $(shell pkg-config --libs libbrotlicommon 2>/dev/null) \
  $(shell pkg-config --libs libzstd 2>/dev/null))
ifeq ($(KRYON_PLATFORM),freebsd)
  # pkg-config omits its default library directory, but the FreeBSD linker
  # does not search the ports prefix automatically.
  CURL_CODEC_LDLIBS := -L/usr/local/lib $(CURL_CODEC_LDLIBS)
endif
KRYON_ZLIB_LDLIB ?= -lz
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=1 -DMAX_CLIPBOARD_BUFFER_LENGTH=1048576
APP_RAYLIB_CONFIG ?= $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
KRYON_STATIC_PACKAGE_EXTERNAL_LIBS ?= $(RAY_LDLIBS) $(KRYON_OPENSSL_SSL_LDLIB) $(KRYON_OPENSSL_CRYPTO_LDLIB) $(KRYON_ZLIB_LDLIB) -lpthread -lm
KRYON_STATIC_PACKAGE_LIBS ?= -lkryon -lraylib -loqs -lcurl -lcmark-gfm-extensions -lcmark-gfm $(KRYON_STATIC_PACKAGE_EXTERNAL_LIBS)
KRYON_STATIC_PACKAGE_CFLAGS ?= -I$${includedir} -DHAS_LIBOQS=1 -DHAS_LIBCURL=1 -DCURL_STATICLIB -DKRYON_HAS_CMARK_GFM=1

# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CPPFLAGS_BASE += $(NIX_CFLAGS_COMPILE)
endif

CPPFLAGS += $(CPPFLAGS_BASE)
# GNU make defines ARFLAGS=rv by default. A Kryon static library must carry
# an index so downstream C/Go linkers can resolve every runtime module.
ARFLAGS = rcs
KRYON_DIR ?= .
KRYON_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
include mk/vendor.mk
include mk/raylib.mk

CPPFLAGS += -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
	-DHAS_LIBCURL=1 $(KRYON_CURL_CFLAGS) \
	$(KRYON_MARKDOWN_CFLAGS)
CPPFLAGS += $(KRYON_NOTIFICATION_CPPFLAGS) $(KRYON_NOTIFICATION_CFLAGS)
LDLIBS += $(KRYON_NOTIFICATION_LDLIBS)

SRCS := $(shell find src -type f -name '*.c' | LC_ALL=C sort)

# The Canvas2D backend is emcc-only (its sources compile to empty
# translation units under native compilers) and only links when
# KRYON_BACKEND=canvas; keep the find from dragging it into the other
# backends' builds when the files happen to be present.
ifneq ($(KRYON_BACKEND),canvas)
SRCS := $(filter-out $(KRYON_CANVAS_SRCS),$(SRCS))
endif
ifneq ($(KRYON_BACKEND),libdraw)
SRCS := $(filter-out $(KRYON_LIBDRAW_SRCS),$(SRCS))
SRCS := $(filter-out src/platform/plan9/%.c,$(SRCS))
endif

SRCS += $(EMBED_ASSETS_C) $(KRYON_BACKEND_SRCS)
KRYON_PUBLIC_HEADERS := $(wildcard include/*.h)

# Drop the Box2D physics sources when physics is disabled (UI-only builds).
# Keep in sync with KRYON_PHYSICS_SRCS in mk/vendor.mk.
KRYON_PHYSICS_SRCS_REL := src/scene/physics_world.c src/scene/node_body2d.c \
	src/scene/node_area2d.c src/scene/node_collision_shape2d.c
ifeq ($(KRYON_WITH_PHYSICS),0)
SRCS := $(filter-out $(KRYON_PHYSICS_SRCS_REL),$(SRCS))
endif

SYSTEM_THEME_PKG := $(shell if pkg-config --exists gtk+-3.0 2>/dev/null; then printf '%s' gtk+-3.0; fi)
ifneq ($(strip $(SYSTEM_THEME_PKG)),)
    CPPFLAGS += $(shell pkg-config --cflags $(SYSTEM_THEME_PKG)) -DSYSTEM_THEME_GTK
    LDLIBS += $(shell pkg-config --libs $(SYSTEM_THEME_PKG))
endif

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
LIB = $(BUILD_DIR)/libkryon.a
KSYNC_ACCOUNT_TEST = $(BUILD_DIR)/tests/ksync_account_test
KSYNC_SYNC_TEST = $(BUILD_DIR)/tests/ksync_sync_test
KSYNC_CRYPTO_TEST = $(BUILD_DIR)/tests/ksync_crypto_test
TRANSITION_TEST = $(BUILD_DIR)/tests/transition_test
FILE_DIALOG_BACKEND_TEST = $(BUILD_DIR)/tests/file_dialog_backend_test
DESKTOP_TEST = $(BUILD_DIR)/tests/desktop_test
LINUX_DESKTOP_PACKAGE_TEST = $(BUILD_DIR)/tests/linux_desktop_package.ok
MARKDOWN_TEST = $(BUILD_DIR)/tests/markdown_test
RAYLIB_COMPAT_TEST = $(BUILD_DIR)/tests/raylib_compat_test
LIBDRAW_SMOKE_TEST = $(BUILD_DIR)/tests/libdraw_smoke_test
LIBDRAW_HIERARCHY_TEST = $(BUILD_DIR)/tests/libdraw_hierarchy_test
UI_TK_TEST = $(BUILD_DIR)/tests/ui_tk_test
UI_PRIMARY_SELECTION_TEST = $(BUILD_DIR)/tests/ui_primary_selection_test
DROPDOWN_LAYOUT_TEST = $(BUILD_DIR)/tests/dropdown_layout_test
DROPDOWN_THEME_SCREEN_TEST = $(BUILD_DIR)/tests/dropdown_theme_screen_test
BOTTOM_NAV_ICON_COLOR_TEST = $(BUILD_DIR)/tests/bottom_nav_icon_color_test
PREVIEW_TEST = $(BUILD_DIR)/tests/preview_test
PLATFORM_THREAD_TEST = $(BUILD_DIR)/tests/platform_thread_test
OPEN_URI_TEST = $(BUILD_DIR)/tests/open_uri_test
UI_TEXT_EDIT_TEST = $(BUILD_DIR)/tests/ui_text_edit_test
UI_TREE_API_TEST = $(BUILD_DIR)/tests/ui_tree_api_test
UI_WINDOW_TEST = $(BUILD_DIR)/tests/ui_window_test
SYSTEM_THEME_TEST = $(BUILD_DIR)/tests/system_theme_test
CURSOR_INTENT_TEST = $(BUILD_DIR)/tests/cursor_intent_test
TEXT_INPUT_PLATFORM_TEST = $(BUILD_DIR)/tests/text_input_platform_test
UI_WINDOW_SDL_CHECK = $(BUILD_DIR)/check/ui_window_sdl.o
TEXT_INPUT_PERF_TEST = $(BUILD_DIR)/tests/text_input_perf_test
TEXT_INPUT_PRECISION_TEST = $(BUILD_DIR)/tests/text_input_precision_test
SCENE_TREE_TEST = $(BUILD_DIR)/tests/scene_tree_test
SCENE_PROPERTY_TEST = $(BUILD_DIR)/tests/scene_property_test
ANIMATION_TEST = $(BUILD_DIR)/tests/animation_test
KIR_TEST = $(BUILD_DIR)/tests/kir_test
K2IR_TEST = $(BUILD_DIR)/tests/k2ir.ok
KRB_WALK_TEST = $(BUILD_DIR)/tests/krb_walk_test
KRB_MOUNT_TEST = $(BUILD_DIR)/tests/krb_mount_test
TERMINAL_TEST = $(BUILD_DIR)/tests/terminal_test
KRY_JSON_TEST = $(BUILD_DIR)/tests/kry_json_test
KRY_HTTP_TEST = $(BUILD_DIR)/tests/kry_http_test
RUNTIME_ASSETS_TEST = $(BUILD_DIR)/tests/runtime_assets_test
KRY_UPDATE_TEST = $(BUILD_DIR)/tests/kry_update_test
KRY_SHA256_TEST = $(BUILD_DIR)/tests/kry_sha256_test
LOCALE_TEST = $(BUILD_DIR)/tests/locale_test
KRY_UPDATE_FLOW_TEST = $(BUILD_DIR)/tests/kry_update_flow_test
SFS_TEST = $(BUILD_DIR)/tests/sfs_test
RAYLIB_COMPAT_LDLIBS ?= $(KRYON_BACKEND_LDLIBS) -lpthread -lm $(if $(filter linux,$(KRYON_PLATFORM)),-ldl -lrt,)

.PHONY: all clean tools examples-run font-assets font-subsets docs-site test spec-test perf-text-input perf-text-input-site bsd-check submodule-urls-check kryon-compat kryon-compat-check kryon-boundary-check public-api-names-check version release-check dist-static check-static-package dist-tools check-tools-package install install-static k2c k2g canvas-test canvas-audio-test libdraw-test libdraw-matrix-check libdraw-matrix-check-internal conformance-matrix-check renderer-matrix-check widget-matrix-check krb-web-matrix-check runtime-matrix-check downstream-matrix-check krb-web krb-sdl icons-generate

k2c: $(K2C)
k2g: $(K2G)

all: $(LIB) $(K2C) $(K2G) $(K2IR) $(K2B) $(KT) $(KRYON_PREVIEW) $(KRYON_CMD)

tools: $(K2C) $(K2G) $(K2IR) $(K2B) $(KT) $(KRYON_PREVIEW) $(KRYON_CMD) $(KRB_RUN) $(KRB_SDL)

install: $(KT) $(KRYON_CMD)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(KT) $(DESTDIR)$(BINDIR)/kt
	$(INSTALL) -m 755 $(KRYON_CMD) $(DESTDIR)$(BINDIR)/kryon

examples-run:
	@$(MAKE) -C examples run

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples web-clean

# SDL2 desktop host: kry_sw renders, SDL2 owns window/input/presentation.
# Uses pkg-config sdl2 (on omega: PKG_CONFIG_PATH=~/.local/sdl2/lib/pkgconfig).
KRB_SDL_CFLAGS = $(shell PKG_CONFIG_PATH=$(HOME)/.local/sdl2/lib/pkgconfig:$(PKG_CONFIG_PATH) pkg-config --cflags sdl2 2>/dev/null)
KRB_SDL_LDLIBS = $(shell PKG_CONFIG_PATH=$(HOME)/.local/sdl2/lib/pkgconfig:$(PKG_CONFIG_PATH) pkg-config --libs sdl2 2>/dev/null)

krb-sdl: $(KRB_SDL)

$(KRB_SDL): cmd/krb-sdl/main.c cmd/krb-run/png_write.c cmd/krb-run/png_write.h $(KRY_SW_SRCS) $(KRY_SW_HDRS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Iinclude -Icmd/krb-run $(KRB_SDL_CFLAGS) -o $@ \
		cmd/krb-sdl/main.c cmd/krb-run/png_write.c $(KRY_SW_SRCS) \
		$(KRB_SDL_LDLIBS) -lm

canvas-test:
	sh tests/canvas_backend_test.sh
	sh tests/canvas_audio_test.sh

canvas-audio-test:
	sh tests/canvas_audio_test.sh

libdraw-test:
	sh tests/libdraw_backend_test.sh
	sh tests/libdraw_9c_test.sh

raylib-matrix-check: $(K2C)
	$(MAKE) --no-print-directory BUILD_DIR=$(BUILD_DIR)-raylib KRYON_BACKEND=raylib raylib-matrix-check-internal

raylib-matrix-check-internal: $(K2C) $(LIB) $(KRYON_BACKEND_LIBS)
	python3 scripts/conformance-matrix.py --verify-raylib-c-visuals \
		--k2c "$(K2C)" \
		--cc "$(CC)" \
		--cppflags "$(CPPFLAGS)" \
		--cflags "$(CFLAGS)" \
		--ldinputs "$(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(KRYON_LIBOQS_A) $(KRYON_CURL_LDLIBS) $(KRYON_MARKDOWN_LDLIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS)"

libdraw-matrix-check: $(K2C)
	$(MAKE) --no-print-directory BUILD_DIR=$(BUILD_DIR)-libdraw KRYON_BACKEND=libdraw libdraw-matrix-check-internal

libdraw-matrix-check-internal: $(LIB)
	python3 scripts/conformance-matrix.py --verify-libdraw-c-visuals \
		--cc "$(CC)" \
		--cppflags "$(CPPFLAGS)" \
		--cflags "$(CFLAGS)" \
		--ldinputs "$(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(KRYON_BACKEND_LDLIBS) $(KRYON_LIBOQS_A) $(KRYON_CURL_LDLIBS) $(KRYON_MARKDOWN_LDLIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS)" \
		--plan9port-dir "$(PLAN9PORT_DIR)"

# Native web host for KRB cartridges: kry_sw rasterizer compiled to wasm,
# blitted to ImageData (pixel-identical to the native headless renderer).
# Needs emcc on PATH (e.g. `source ~/emsdk/emsdk_env.sh`).
krb-web: $(KRY_SW_SRCS) $(KRY_SW_HDRS) cmd/krb-web/main.c cmd/krb-web/shell.html | $(KRB_WEB_DIR)
	$(EMCC) -Wall -Wextra -Os -Iinclude \
		-sEXPORTED_FUNCTIONS=_krb_web_mouse,_krb_web_button,_krb_web_wheel,_krb_web_text,_krb_web_start,_main \
		-sEXPORTED_RUNTIME_METHODS=FS \
		-sALLOW_MEMORY_GROWTH=1 \
		--shell-file cmd/krb-web/shell.html \
		-o $(KRB_WEB) \
		cmd/krb-web/main.c $(KRY_SW_SRCS)
	@echo "krb-web player: serve $(KRB_WEB_DIR) (e.g. python3 -m http.server -d $(KRB_WEB_DIR))"

$(KRB_WEB_DIR):
	mkdir -p $@

docs-site:
	rm -rf $(SITE_BUILD_DIR)
	mkdir -p $(SITE_BUILD_DIR)
	cp -R $(SITE_DIR)/. $(SITE_BUILD_DIR)/
	cp -R icons $(SITE_BUILD_DIR)/
	cp -R $(SITE_DIR)/cursors $(SITE_BUILD_DIR)/
	if [ -n "$(SHOWCASE_PROJECTS_DIR)" ]; then \
		python3 scripts/update-showcase.py --projects-dir "$(SHOWCASE_PROJECTS_DIR)" --output $(SITE_BUILD_DIR)/showcase-data.json --banner-dir $(SITE_BUILD_DIR)/showcase; \
	else \
		python3 scripts/update-showcase.py --output $(SITE_BUILD_DIR)/showcase-data.json --banner-dir $(SITE_BUILD_DIR)/showcase; \
	fi
	sh scripts/build-site-web-ide.sh $(SITE_BUILD_DIR)
	sh scripts/render-api-html.sh docs/API.md $(SITE_DIR)/api-template.html $(SITE_BUILD_DIR)/api.html
	rm -f $(SITE_BUILD_DIR)/api-template.html
	test -f $(SITE_BUILD_DIR)/language.html
	test -f $(SITE_BUILD_DIR)/benchmarks.html
	test -f $(SITE_BUILD_DIR)/matrices.html
	test -f $(SITE_BUILD_DIR)/renderers.html

spec-test: $(K2IR) $(K2C) $(K2G) $(K2B)
	sh tests/spec/spec_test.sh . $(BUILD_DIR)

runtime-parity-check:
	sh tests/runtime_parity_test.sh .

feature-matrix-docs-check:
	sh tests/feature_matrix_docs_test.sh .

conformance-matrix-check: $(K2IR) $(K2C) $(K2G) $(K2B) $(KRB_RUN) $(KRB_SDL)
	sh tests/conformance_matrix_test.sh .

renderer-matrix-check:
	python3 scripts/conformance-matrix.py --verify-renderer-smokes

widget-matrix-check:
	python3 scripts/conformance-matrix.py --verify-widget-coverage

krb-web-matrix-check:
	python3 scripts/conformance-matrix.py --verify-krb-web-visuals

runtime-matrix-check:
	python3 scripts/conformance-matrix.py --verify-runtime-parity

downstream-matrix-check:
	python3 scripts/conformance-matrix.py --verify-downstream

generated-runtime-parity-test: $(K2C) $(K2G) $(LIB) $(KRYON_BACKEND_LIBS)
	sh tests/generated_runtime_parity_test.sh . $(BUILD_DIR) "$(CC)" "$(CPPFLAGS)" "$(CFLAGS)" "$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS)"

test: submodule-urls-check kryon-compat-check kryon-boundary-check public-api-names-check runtime-parity-check feature-matrix-docs-check conformance-matrix-check $(K2C) $(K2G) $(K2IR) $(K2B) $(KT) $(KSYNC_ACCOUNT_TEST) $(KSYNC_SYNC_TEST) $(KSYNC_CRYPTO_TEST) $(TRANSITION_TEST) $(FILE_DIALOG_BACKEND_TEST) $(DESKTOP_TEST) $(LINUX_DESKTOP_PACKAGE_TEST) $(MARKDOWN_TEST) $(RAYLIB_COMPAT_TEST) $(UI_TK_TEST) $(UI_PRIMARY_SELECTION_TEST) $(DROPDOWN_LAYOUT_TEST) $(DROPDOWN_THEME_SCREEN_TEST) $(BOTTOM_NAV_ICON_COLOR_TEST) $(PREVIEW_TEST) $(PLATFORM_THREAD_TEST) $(OPEN_URI_TEST) $(UI_TEXT_EDIT_TEST) $(UI_TREE_API_TEST) $(SCENE_TREE_TEST) $(SCENE_PROPERTY_TEST) $(ANIMATION_TEST) $(KIR_TEST) $(K2IR_TEST) $(KRB_WALK_TEST) $(KRB_MOUNT_TEST) $(KRY_SW_TEST) $(KRB_LOGIC_TEST) $(KRB_ASSET_TEST) $(KRB_CAPS_TEST) $(KRB_RUN) $(TERMINAL_TEST) $(KRY_JSON_TEST) $(KRY_HTTP_TEST) $(RUNTIME_ASSETS_TEST) $(KRY_UPDATE_TEST) $(KRY_UPDATE_FLOW_TEST) $(KRY_SHA256_TEST) $(LOCALE_TEST) $(SFS_TEST) $(UI_WINDOW_TEST) $(SYSTEM_THEME_TEST) $(CURSOR_INTENT_TEST) $(TEXT_INPUT_PLATFORM_TEST) $(UI_WINDOW_SDL_CHECK)
	sh tests/spec/spec_test.sh . $(BUILD_DIR)
	sh tests/k2c_syntax_test.sh $(K2C)
	sh tests/k2g_syntax_test.sh $(K2G)
	sh tests/generated_runtime_parity_test.sh . $(BUILD_DIR) "$(CC)" "$(CPPFLAGS)" "$(CFLAGS)" "$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS)"
	sh tests/kt_cli_test.sh $(KT)
	sh tests/krb_cartridge_test.sh $(K2B) $(KRB_WALK_TEST) .
	sh tests/krb_engine_test.sh $(K2B) $(KRB_RUN) .
	$(KRY_SW_TEST)
	mkdir -p $(BUILD_DIR)/capstore
	mkdir -p $(BUILD_DIR)/tests/caps-fixture
	$(K2B) --root examples -o $(BUILD_DIR)/tests/caps-fixture examples/02_buttons.kry
	KRB_CAP_STORE_DIR=$(BUILD_DIR)/capstore $(KRB_CAPS_TEST) $(BUILD_DIR)/tests/caps-fixture/02_buttons.krb
	$(KRB_MOUNT_TEST)
	$(TERMINAL_TEST)
	$(KRY_JSON_TEST)
	$(KRY_HTTP_TEST)
	$(RUNTIME_ASSETS_TEST)
	$(KRY_UPDATE_TEST)
	$(KRY_UPDATE_FLOW_TEST)
	$(KRY_SHA256_TEST)
	$(LOCALE_TEST)
	$(SFS_TEST)
	$(KSYNC_ACCOUNT_TEST)
	$(KSYNC_SYNC_TEST)
	$(KSYNC_CRYPTO_TEST)
	$(TRANSITION_TEST)
	$(FILE_DIALOG_BACKEND_TEST)
	$(DESKTOP_TEST)
	@cat $(LINUX_DESKTOP_PACKAGE_TEST)
	$(MARKDOWN_TEST)
	$(RAYLIB_COMPAT_TEST)
	$(UI_TK_TEST)
	$(UI_PRIMARY_SELECTION_TEST)
	$(DROPDOWN_LAYOUT_TEST)
	$(DROPDOWN_THEME_SCREEN_TEST)
	$(BOTTOM_NAV_ICON_COLOR_TEST)
	$(PREVIEW_TEST)
	$(PLATFORM_THREAD_TEST)
	$(OPEN_URI_TEST)
	$(UI_TEXT_EDIT_TEST)
	$(UI_TREE_API_TEST)
	$(UI_WINDOW_TEST)
	$(SYSTEM_THEME_TEST)
	$(CURSOR_INTENT_TEST)
	$(KIR_TEST)
	@cat $(K2IR_TEST)

bsd-check:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) test

kryon-compat: $(KRYON_COMPAT_HEADER) $(KRYON_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_WRAPPERS_C)

submodule-urls-check:
	sh scripts/check-submodule-urls.sh

kryon-compat-check: | $(BUILD_DIR)
	sh $(KRYON_COMPAT_GENERATOR) vendor/raylib/src/raylib.h \
		$(BUILD_DIR)/check/kryon_compat.generated.h \
		$(BUILD_DIR)/check/raylib_backend_rename.h \
		$(BUILD_DIR)/check/kryon_raylib_wrappers.c
	cmp $(KRYON_COMPAT_HEADER) $(BUILD_DIR)/check/kryon_compat.generated.h
	sh $(KRYON_COMPAT_SYMBOL_CHECK) vendor/raylib/src/raylib.h \
		$(BUILD_DIR)/check/raylib_backend_rename.h \
		$(BUILD_DIR)/check/kryon_raylib_wrappers.c

kryon-boundary-check:
	sh $(KRYON_BOUNDARY_CHECK) .

public-api-names-check:
	sh tests/public_api_names_test.sh .

$(LIB): $(OBJS) $(KRYON_BACKEND_STAMP) | $(BUILD_DIR) $(KRYON_COMPAT_HEADER) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS) $(KRYON_PHYSICS_DEPS)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(OBJS)

$(KRYON_BACKEND_STAMP): | $(BUILD_DIR)
	rm -f $(BUILD_DIR)/.backend-*
	touch $@

K2C_SRCS := $(sort $(wildcard cmd/k2c/*.c)) cmd/kir/kir.c cmd/kir/kir_parse.c
K2C_HDRS := cmd/k2c/k2c_lower.h cmd/kir/kir.h cmd/kir/kir_parse.h

$(K2C): $(K2C_SRCS) $(K2C_HDRS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Icmd/kir -o $@ $(K2C_SRCS)

K2G_SRCS := $(sort $(wildcard cmd/k2g/*.c)) cmd/kir/kir.c cmd/kir/kir_parse.c
$(K2G): $(K2G_SRCS) cmd/kir/kir.h cmd/kir/kir_parse.h | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Icmd/kir -o $@ $(K2G_SRCS)

K2IR_SRCS := $(sort $(wildcard cmd/k2ir/*.c)) cmd/kir/kir.c cmd/kir/kir_parse.c
$(K2IR): $(K2IR_SRCS) cmd/kir/kir.h cmd/kir/kir_parse.h | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Icmd/kir -o $@ $(K2IR_SRCS)

K2B_SRCS := $(sort $(wildcard cmd/k2b/*.c)) cmd/kir/kir.c cmd/kir/kir_parse.c
$(K2B): $(K2B_SRCS) cmd/kir/kir.h cmd/kir/kir_parse.h | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Iinclude -Icmd/kir -o $@ $(K2B_SRCS) -lm

$(KT): cmd/kt/main.c | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS_BASE) -o $@ cmd/kt/main.c

$(KRYON_PREVIEW): cmd/kryon-preview/main.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ cmd/kryon-preview/main.c \
		-Wl,-export-dynamic \
		-Wl,--whole-archive $(LIB) -Wl,--no-whole-archive \
		$(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(KRYON_BACKEND_LDLIBS) $(KRYON_LIBOQS_A) \
		$(KRYON_CURL_LDLIBS) $(KRYON_MARKDOWN_LDLIBS) \
		$(CURL_CODEC_LDLIBS) $(LDLIBS) -lpthread -lm

$(KRYON_CMD): scripts/kryon.sh | $(BUILD_DIR)/bin
	cp scripts/kryon.sh $@
	chmod 755 $@

version:
	@printf '%s\n' '$(VERSION)'

release-check:
	@test -n '$(VERSION)'
	@grep -q '^## $(VERSION) ' CHANGELOG.md

dist-static: release-check $(STATIC_DIST_ARCHIVE)

check-static-package: $(STATIC_DIST_ARCHIVE)
	sh scripts/check-static-package.sh $(STATIC_DIST_ARCHIVE)

dist-tools: release-check $(TOOLS_DIST_ARCHIVE)

check-tools-package: $(TOOLS_DIST_ARCHIVE)
	sh scripts/check-tools-package.sh $(TOOLS_DIST_ARCHIVE)

install-static: $(STATIC_DIST_ARCHIVE)
	mkdir -p $(DESTDIR)$(PREFIX)
	tar -xzf $(STATIC_DIST_ARCHIVE) -C $(DESTDIR)$(PREFIX) --strip-components=1

$(STATIC_DIST_ARCHIVE): $(LIB) $(RAYLIB_A) $(KRYON_LIBOQS_A) $(KRYON_CURL_A) $(KRYON_MARKDOWN_DEPS) README.md LICENSE THIRD_PARTY_NOTICES.md CHANGELOG.md docs/API.md examples/package/minimal.c examples/package/markdown.c scripts/check-static-package.sh
	rm -rf $(STATIC_DIST_ROOT)
	mkdir -p $(STATIC_DIST_ROOT)/include $(STATIC_DIST_ROOT)/lib $(STATIC_DIST_ROOT)/lib/pkgconfig $(STATIC_DIST_ROOT)/lib/cmake/kryon $(STATIC_DIST_ROOT)/share/doc/kryon $(STATIC_DIST_ROOT)/share/licenses/kryon $(STATIC_DIST_ROOT)/examples $(DIST_DIR)
	cp -R include/. $(STATIC_DIST_ROOT)/include/
	cp $(LIB) $(RAYLIB_A) $(KRYON_LIBOQS_A) $(KRYON_CURL_A) $(KRYON_MARKDOWN_DEPS) $(STATIC_DIST_ROOT)/lib/
	cp README.md $(STATIC_DIST_ROOT)/
	cp LICENSE $(STATIC_DIST_ROOT)/
	cp LICENSE THIRD_PARTY_NOTICES.md $(STATIC_DIST_ROOT)/share/licenses/kryon/
	cp CHANGELOG.md docs/API.md $(STATIC_DIST_ROOT)/share/doc/kryon/
	cp examples/package/*.c $(STATIC_DIST_ROOT)/examples/
	git submodule status > $(STATIC_DIST_ROOT)/SUBMODULES.txt
	printf '%s\n' '$(VERSION)' > $(STATIC_DIST_ROOT)/VERSION
	printf '%s\n' \
		'{' \
		'  "name": "kryon",' \
		'  "version": "$(VERSION)",' \
		'  "target": "$(KRYON_PLATFORM)-$(KRYON_ARCH)",' \
		'  "compiler": "$(CC)",' \
		'  "static_libraries": ["libkryon.a", "libraylib.a", "liboqs.a", "libcurl.a", "libcmark-gfm-extensions.a", "libcmark-gfm.a"],' \
		'  "external_libraries": "$(KRYON_STATIC_PACKAGE_EXTERNAL_LIBS)",' \
		'  "pkg_config": "lib/pkgconfig/kryon.pc"' \
		'}' > $(STATIC_DIST_ROOT)/manifest.json
	printf '%s\n' \
		'prefix=$${pcfiledir}/../..' \
		'exec_prefix=$${prefix}' \
		'libdir=$${prefix}/lib' \
		'includedir=$${prefix}/include' \
		'' \
		'Name: Kryon' \
		'Description: Kryon C support library for raylib-style applications' \
		'Version: $(VERSION)' \
		'Cflags: $(KRYON_STATIC_PACKAGE_CFLAGS)' \
		'Libs: -L$${libdir} $(KRYON_STATIC_PACKAGE_LIBS)' \
		> $(STATIC_DIST_ROOT)/lib/pkgconfig/kryon.pc
	printf '%s\n' \
		'include(CMakeFindDependencyMacro)' \
		'get_filename_component(KRYON_PACKAGE_PREFIX "$${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)' \
		'add_library(Kryon::kryon STATIC IMPORTED)' \
		'set_target_properties(Kryon::kryon PROPERTIES IMPORTED_LOCATION "$${KRYON_PACKAGE_PREFIX}/lib/libkryon.a" INTERFACE_INCLUDE_DIRECTORIES "$${KRYON_PACKAGE_PREFIX}/include" INTERFACE_COMPILE_DEFINITIONS "HAS_LIBOQS=1;HAS_LIBCURL=1;CURL_STATICLIB;KRYON_HAS_CMARK_GFM=1")' \
		'add_library(Kryon::raylib STATIC IMPORTED)' \
		'set_target_properties(Kryon::raylib PROPERTIES IMPORTED_LOCATION "$${KRYON_PACKAGE_PREFIX}/lib/libraylib.a")' \
		'set(Kryon_PACKAGE_LIBS "$${KRYON_PACKAGE_PREFIX}/lib/liboqs.a" "$${KRYON_PACKAGE_PREFIX}/lib/libcurl.a" "$${KRYON_PACKAGE_PREFIX}/lib/libcmark-gfm-extensions.a" "$${KRYON_PACKAGE_PREFIX}/lib/libcmark-gfm.a")' \
		'set_target_properties(Kryon::kryon PROPERTIES INTERFACE_LINK_LIBRARIES "Kryon::raylib;$${Kryon_PACKAGE_LIBS}")' \
		'set(Kryon_LIBRARIES Kryon::kryon Kryon::raylib $${Kryon_PACKAGE_LIBS})' \
		> $(STATIC_DIST_ROOT)/lib/cmake/kryon/KryonConfig.cmake
	tar -C $(BUILD_DIR)/dist -czf $@ kryon-$(VERSION)-static

$(TOOLS_DIST_ARCHIVE): tools README.md LICENSE THIRD_PARTY_NOTICES.md scripts/check-tools-package.sh
	rm -rf $(TOOLS_DIST_ROOT)
	mkdir -p $(TOOLS_DIST_ROOT)/bin $(DIST_DIR)
	cp $(K2C) $(K2G) $(K2IR) $(K2B) $(KT) $(KRYON_PREVIEW) $(KRYON_CMD) $(KRB_RUN) $(KRB_SDL) $(TOOLS_DIST_ROOT)/bin/
	chmod 755 $(TOOLS_DIST_ROOT)/bin/*
	printf '%s\n' '$(VERSION)' > $(TOOLS_DIST_ROOT)/VERSION
	cp README.md LICENSE THIRD_PARTY_NOTICES.md $(TOOLS_DIST_ROOT)/
	printf '%s\n' \
		'{' \
		'  "name": "kryon-tools",' \
		'  "version": "$(VERSION)",' \
		'  "target": "$(KRYON_PLATFORM)-$(KRYON_ARCH)",' \
		'  "binaries": ["k2c", "k2g", "k2ir", "k2b", "kt", "kryon", "kryon-preview", "krb-run", "krb-sdl"]' \
		'}' > $(TOOLS_DIST_ROOT)/manifest.json
	tar -C $(BUILD_DIR)/dist -czf $@ $(notdir $(TOOLS_DIST_ROOT))

$(KSYNC_ACCOUNT_TEST): tests/ksync_account_test.c src/ksync/ksync_account.c src/ksync/ksync_crypto.c include/ksync_account.h include/ksync_crypto.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/ksync_account_test.c src/ksync/ksync_account.c src/ksync/ksync_crypto.c \
		$(KRYON_LIBOQS_A) -lm -o $@

$(KSYNC_SYNC_TEST): tests/ksync_sync_test.c src/ksync/ksync_sync.c src/ksync/ksync_account.c src/ksync/ksync_crypto.c include/ksync_sync.h include/ksync_account.h include/ksync_crypto.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/ksync_sync_test.c src/ksync/ksync_sync.c src/ksync/ksync_account.c \
		src/ksync/ksync_crypto.c \
		$(KRYON_LIBOQS_A) -lm -o $@

$(KSYNC_CRYPTO_TEST): tests/ksync_crypto_test.c src/ksync/ksync_crypto.c include/ksync_crypto.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ksync_crypto_test.c src/ksync/ksync_crypto.c -o $@

$(TRANSITION_TEST): tests/transition_test.c src/ui/ui_transition.c include/ui_transition.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/transition_test.c src/ui/ui_transition.c -o $@

$(MARKDOWN_TEST): tests/markdown_test.c src/markdown.c include/markdown.h $(KRYON_MARKDOWN_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(KRYON_MARKDOWN_CFLAGS) \
		tests/markdown_test.c src/markdown.c \
		$(KRYON_MARKDOWN_LDLIBS) -o $@

$(FILE_DIALOG_BACKEND_TEST): tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c include/file_dialog.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c $(LDLIBS) -o $@

$(DESKTOP_TEST): tests/desktop_test.c src/platform/desktop/desktop.c src/notification/notification.c include/desktop.h include/notification.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/desktop_test.c src/platform/desktop/desktop.c src/notification/notification.c $(LDLIBS) -o $@

$(LINUX_DESKTOP_PACKAGE_TEST): tests/linux_desktop_package_test.sh scripts/package-linux-desktop.sh | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	sh tests/linux_desktop_package_test.sh . > $@

$(RAYLIB_COMPAT_TEST): tests/raylib_compat_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/raylib_compat_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) \
		-o $@

$(LIBDRAW_SMOKE_TEST): tests/libdraw_smoke_main.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/libdraw_smoke_main.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(LIBDRAW_HIERARCHY_TEST): tests/libdraw_hierarchy_main.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/libdraw_hierarchy_main.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(UI_TK_TEST): tests/ui_tk_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_tk_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(SYSTEM_THEME_TEST): tests/system_theme_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/system_theme_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(CURSOR_INTENT_TEST): tests/cursor_intent_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/cursor_intent_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(TEXT_INPUT_PLATFORM_TEST): tests/text_input_platform_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/text_input_platform_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(UI_PRIMARY_SELECTION_TEST): tests/ui_primary_selection_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_primary_selection_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(DROPDOWN_LAYOUT_TEST): tests/dropdown_layout_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/dropdown_layout_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(DROPDOWN_THEME_SCREEN_TEST): tests/dropdown_theme_screen_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/dropdown_theme_screen_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(BOTTOM_NAV_ICON_COLOR_TEST): tests/bottom_nav_icon_color_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/bottom_nav_icon_color_test.c \
		-Wl,--wrap=DrawTexturePro \
		-Wl,--wrap=DrawRectangleRec \
		-Wl,--wrap=DrawRectangleRounded \
		-Wl,--wrap=SetMouseCursor \
		-Wl,--wrap=GetMousePosition \
		-Wl,--wrap=IsMouseButtonReleased \
		-Wl,--wrap=IsMouseButtonDown \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(PREVIEW_TEST): tests/preview_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/preview_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(PLATFORM_THREAD_TEST): tests/platform_thread_test.c src/platform/platform_thread.c include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/platform_thread_test.c \
		src/platform/platform_thread.c -lpthread -o $@

$(OPEN_URI_TEST): tests/open_uri_test.c src/platform/open_uri.c include/kry_uri.h include/kryon_compat.generated.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/open_uri_test.c src/platform/open_uri.c -o $@

$(UI_TEXT_EDIT_TEST): tests/ui_text_edit_test.c src/ui/ui_text_edit.c include/kryon.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_text_edit_test.c src/ui/ui_text_edit.c -o $@

$(UI_TREE_API_TEST): tests/ui_tree_api_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_tree_api_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(UI_WINDOW_TEST): tests/ui_window_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_window_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

# Compile-only coverage for the SDL secondary-window presenter (Wayland and
# SDL-bundled platforms). The default Linux/FreeBSD build takes the X11 path,
# so without this the UI_WINDOW_HAVE_SDL branch would rot uncompiled.
$(UI_WINDOW_SDL_CHECK): src/ui/ui_window.c $(KRYON_COMPAT_HEADER) $(KRYON_BACKEND_RENAME_HEADER) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DUI_WINDOW_HAVE_SDL \
		$(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS) -I$(RAY_SDL_INCLUDE_DIR) \
		-c $< -o $@

$(TEXT_INPUT_PERF_TEST): tests/text_input_perf_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/text_input_perf_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(TEXT_INPUT_PRECISION_TEST): tests/text_input_precision_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/text_input_precision_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

perf-text-input: $(K2IR) $(K2C) $(K2G) $(K2B) $(TEXT_INPUT_PERF_TEST) $(TEXT_INPUT_PRECISION_TEST)
	sh tests/text_input_perf.sh . $(BUILD_DIR)
	$(TEXT_INPUT_PRECISION_TEST)

perf-text-input-site: $(K2IR) $(K2C) $(K2G) $(K2B) $(TEXT_INPUT_PERF_TEST) $(TEXT_INPUT_PRECISION_TEST)
	mkdir -p $(BUILD_DIR)
	{ sh tests/text_input_perf.sh . $(BUILD_DIR); $(TEXT_INPUT_PRECISION_TEST); } | tee $(BUILD_DIR)/text-input-perf.jsonl
	python3 scripts/render_benchmarks.py docs/site/benchmarks.json $(BUILD_DIR)/text-input-perf.jsonl

$(SCENE_TREE_TEST): tests/scene_tree_test.c $(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/scene_tree_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(SCENE_PROPERTY_TEST): tests/scene_property_test.c $(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/scene_property_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(ANIMATION_TEST): tests/animation_test.c $(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/animation_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(KIR_TEST): tests/kir_test.c cmd/kir/kir.c cmd/kir/kir.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Icmd/kir tests/kir_test.c cmd/kir/kir.c -o $@

$(K2IR_TEST): tests/k2ir_test.sh $(K2IR) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	TMPDIR=$(BUILD_DIR) sh tests/k2ir_test.sh $(K2IR) . > $@

$(KRB_WALK_TEST): tests/krb_walk_test.c src/krb/krb.c src/backend/kry_backend.c include/krb.h include/kry_backend.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/krb_walk_test.c src/krb/krb.c \
		src/backend/kry_backend.c -o $@

$(KRB_MOUNT_TEST): tests/krb_mount_test.c src/krb/krb.c src/backend/kry_backend.c include/krb.h include/kry_backend.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/krb_mount_test.c src/krb/krb.c \
		src/backend/kry_backend.c -o $@

KRY_SW_SRCS = src/krb/krb.c src/krb/krb_caps.c src/backend/kry_backend.c \
	src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend_rec.c
KRY_SW_HDRS = include/krb.h include/kry_backend.h include/kry_sw.h \
	include/kry_backend_rec.h

$(KRB_RUN): cmd/krb-run/main.c cmd/krb-run/png_write.c cmd/krb-run/png_write.h $(KRY_SW_SRCS) $(KRY_SW_HDRS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Iinclude -Icmd/krb-run -o $@ cmd/krb-run/main.c \
		cmd/krb-run/png_write.c $(KRY_SW_SRCS) -lm

$(KRY_SW_TEST): tests/kry_sw_test.c src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend_rec.c src/backend/kry_backend.c include/kry_sw.h include/kry_backend_rec.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/kry_sw_test.c src/backend/kry_sw.c \
		src/backend/kry_sw_png.c \
		src/backend/kry_backend_rec.c src/backend/kry_backend.c -o $@ -lm

$(KRB_LOGIC_TEST): tests/krb_logic_test.c src/krb/krb.c src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend.c include/krb.h include/kry_sw.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/krb_logic_test.c src/krb/krb.c \
		src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend.c -o $@ -lm

$(KRB_ASSET_TEST): tests/krb_asset_test.c src/krb/krb.c src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend.c include/krb.h include/kry_sw.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/krb_asset_test.c src/krb/krb.c \
		src/backend/kry_sw.c src/backend/kry_sw_png.c src/backend/kry_backend.c -o $@ -lm

$(KRB_CAPS_TEST): tests/krb_caps_test.c src/krb/krb.c src/krb/krb_caps.c include/krb.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/krb_caps_test.c src/krb/krb.c \
		src/krb/krb_caps.c src/backend/kry_backend.c -o $@ -lm

$(TERMINAL_TEST): tests/terminal_test.c src/kry_std/terminal.c include/terminal.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/terminal_test.c src/kry_std/terminal.c -o $@

$(SFS_TEST): tests/sfs_test.c $(LIB) $(KRYON_BACKEND_LIBS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/sfs_test.c \
		$(LIB) $(KRYON_BACKEND_LIBS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(KRY_JSON_TEST): tests/kry_json_test.c src/kry_std/kry_json.c include/kry_json.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/kry_json_test.c src/kry_std/kry_json.c -o $@

$(KRY_HTTP_TEST): tests/kry_http_test.c src/kry_std/kry_http.c src/platform/platform_thread.c include/kry_http.h include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(KRYON_CURL_CFLAGS) tests/kry_http_test.c src/kry_std/kry_http.c src/platform/platform_thread.c $(KRYON_CURL_LDLIBS) $(KRYON_CURL_TRANSITIVE_LDLIBS) -o $@

$(RUNTIME_ASSETS_TEST): tests/runtime_assets_test.c src/runtime_assets/runtime_assets.c src/platform/platform_thread.c include/runtime_assets.h include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(KRYON_CURL_CFLAGS) tests/runtime_assets_test.c src/runtime_assets/runtime_assets.c src/platform/platform_thread.c $(KRYON_CURL_LDLIBS) $(KRYON_CURL_TRANSITIVE_LDLIBS) -o $@

$(KRY_UPDATE_TEST): tests/kry_update_test.c src/kry_std/kry_update.c src/kry_std/kry_json.c src/kry_std/kry_http.c src/kry_std/kry_filesystem.c src/kry_std/kry_sha256.c src/platform/platform_thread.c include/kry_update.h include/kry_json.h include/kry_http.h include/kry_filesystem.h include/kry_sha256.h include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(KRYON_CURL_CFLAGS) tests/kry_update_test.c src/kry_std/kry_update.c src/kry_std/kry_json.c src/kry_std/kry_http.c src/kry_std/kry_filesystem.c src/kry_std/kry_sha256.c src/platform/platform_thread.c $(KRYON_CURL_LDLIBS) $(KRYON_CURL_TRANSITIVE_LDLIBS) -o $@

$(KRY_UPDATE_FLOW_TEST): tests/kry_update_flow_test.c src/kry_std/kry_update_flow.c src/kry_std/kry_update.c src/kry_std/kry_json.c src/kry_std/kry_http.c src/kry_std/kry_filesystem.c src/kry_std/kry_sha256.c src/platform/platform_thread.c include/kry_update_flow.h include/kry_update.h include/kry_json.h include/kry_http.h include/kry_filesystem.h include/kry_sha256.h include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(KRYON_CURL_CFLAGS) tests/kry_update_flow_test.c src/kry_std/kry_update_flow.c src/kry_std/kry_update.c src/kry_std/kry_json.c src/kry_std/kry_http.c src/kry_std/kry_filesystem.c src/kry_std/kry_sha256.c src/platform/platform_thread.c $(KRYON_CURL_LDLIBS) $(KRYON_CURL_TRANSITIVE_LDLIBS) -o $@

$(KRY_SHA256_TEST): tests/kry_sha256_test.c src/kry_std/kry_sha256.c include/kry_sha256.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/kry_sha256_test.c src/kry_std/kry_sha256.c -o $@

$(LOCALE_TEST): tests/locale_test.c src/core/locale.c include/locale.h include/embedded_assets.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/locale_test.c src/core/locale.c -o $@


$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/ui_icons.h
	sh scripts/embed-icons.sh "$(ICON_DIR)" $@

src/ui/ui_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

icons-generate: scripts/make_icons.py scripts/embed-icons.sh
	python3 scripts/make_icons.py
	sh scripts/embed-icons.sh "$(ICON_DIR)" $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) scripts/embed-assets.sh include/embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS)

$(BUILD_DIR)/%.o: src/%.c $(KRYON_PUBLIC_HEADERS) $(KRYON_BACKEND_STAMP) | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c $(KRYON_PUBLIC_HEADERS) $(KRYON_BACKEND_STAMP) | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/bin:
	mkdir -p $@

font-assets:
	@printf '%s\n' 'font assets are checked in or supplied by downstream apps'

font-subsets:
	@if [ -z "$(strip $(FONT_SUBSET_CORPUS))" ]; then \
		echo "Set FONT_SUBSET_CORPUS to one or more files/directories"; \
		exit 1; \
	fi
	sh scripts/subset-fonts.sh "$(FONT_SUBSET_OUT_DIR)" "$(FONT_SUBSET_SOURCE_DIR)" "$(FONT_SUBSET_PREFIX)" $(FONT_SUBSET_CORPUS)
