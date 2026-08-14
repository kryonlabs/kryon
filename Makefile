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
K2C = $(BUILD_DIR)/bin/k2c
K2IR = $(BUILD_DIR)/bin/k2ir
K2B = $(BUILD_DIR)/bin/k2b
KT = $(BUILD_DIR)/bin/kt
KRYON_PREVIEW = $(BUILD_DIR)/bin/kryon-preview
KRYON_CMD = $(BUILD_DIR)/bin/kryon
LEGACY_K2C = $(BUILD_ROOT)/bin/k2c
LEGACY_K2IR = $(BUILD_ROOT)/bin/k2ir
LEGACY_K2B = $(BUILD_ROOT)/bin/k2b
LEGACY_KT = $(BUILD_ROOT)/bin/kt
LEGACY_KRYON_CMD = $(BUILD_ROOT)/bin/kryon
BINDIR ?= $(PREFIX)/bin
INSTALL ?= install
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude -I$(KRYON_DIR)/vendor/clay $(KRYON_PHYSICS_CPPFLAGS)
ICON_DIR ?= icons language payments platforms tiles pfp
ICON_FILES = $(foreach dir,$(ICON_DIR),$(wildcard $(dir)/*.png))
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

# Graphics/input backend. The kryon surface (kryon_compat.generated.h) is
# backend-neutral; the concrete implementation is selected at link time here.
#   raylib  -> generated raylib forwarders + libraylib.a   (default, unchanged)
#   canvas  -> src/backend/canvas_backend.c (HTML5 Canvas2D; no raylib)
#   null    -> generated zero-return stubs  (no-ops; for headless tests)
KRYON_BACKEND ?= raylib
ifeq ($(KRYON_BACKEND),raylib)
  KRYON_BACKEND_SRCS = $(KRYON_RAYLIB_WRAPPERS_C)
else ifeq ($(KRYON_BACKEND),canvas)
  KRYON_BACKEND_SRCS = src/backend/canvas_backend.c
else ifeq ($(KRYON_BACKEND),null)
  KRYON_BACKEND_SRCS = $(KRYON_NULL_BACKEND_C)
else
  $(error Unknown KRYON_BACKEND '$(KRYON_BACKEND)' (expected raylib, canvas, or null))
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
ARFLAGS ?= rcs
KRYON_DIR ?= .
KRYON_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
include mk/vendor.mk
include mk/raylib.mk

CPPFLAGS += -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
	-DHAS_LIBCURL=1 $(KRYON_CURL_CFLAGS) \
	$(KRYON_MARKDOWN_CFLAGS)

SRCS := $(shell find src -type f -name '*.c' | LC_ALL=C sort)

SRCS += $(EMBED_ASSETS_C) $(KRYON_BACKEND_SRCS)

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
TRANSITION_TEST = $(BUILD_DIR)/tests/transition_test
FILE_DIALOG_BACKEND_TEST = $(BUILD_DIR)/tests/file_dialog_backend_test
MARKDOWN_TEST = $(BUILD_DIR)/tests/markdown_test
RAYLIB_COMPAT_TEST = $(BUILD_DIR)/tests/raylib_compat_test
UI_TK_TEST = $(BUILD_DIR)/tests/ui_tk_test
PREVIEW_TEST = $(BUILD_DIR)/tests/preview_test
PLATFORM_THREAD_TEST = $(BUILD_DIR)/tests/platform_thread_test
UI_TEXT_EDIT_TEST = $(BUILD_DIR)/tests/ui_text_edit_test
UI_TREE_API_TEST = $(BUILD_DIR)/tests/ui_tree_api_test
SCENE_TREE_TEST = $(BUILD_DIR)/tests/scene_tree_test
SCENE_PROPERTY_TEST = $(BUILD_DIR)/tests/scene_property_test
ANIMATION_TEST = $(BUILD_DIR)/tests/animation_test
KIR_TEST = $(BUILD_DIR)/tests/kir_test
K2IR_TEST = $(BUILD_DIR)/tests/k2ir.ok
KRB_WALK_TEST = $(BUILD_DIR)/tests/krb_walk_test
KRB_MOUNT_TEST = $(BUILD_DIR)/tests/krb_mount_test
KRY_TERM_TEST = $(BUILD_DIR)/tests/kry_term_test
RAYLIB_COMPAT_LDLIBS ?= $(RAY_LDLIBS) -lpthread -lm $(if $(filter linux,$(KRYON_PLATFORM)),-ldl -lrt,)

.PHONY: all clean tools examples-run font-assets font-subsets docs-site test bsd-check kryon-compat kryon-compat-check kryon-boundary-check version release-check dist-static check-static-package install install-static

ifneq ($(BUILD_DIR),$(BUILD_ROOT))
.PHONY: $(LEGACY_K2C) $(LEGACY_K2IR) $(LEGACY_K2B) $(LEGACY_KT) $(LEGACY_KRYON_CMD)

$(LEGACY_K2C): $(K2C) | $(BUILD_ROOT)/bin
	$(INSTALL) -m 755 $(K2C) $@

$(LEGACY_K2IR): $(K2IR) | $(BUILD_ROOT)/bin
	$(INSTALL) -m 755 $(K2IR) $@

$(LEGACY_K2B): $(K2B) | $(BUILD_ROOT)/bin
	$(INSTALL) -m 755 $(K2B) $@

$(LEGACY_KT): $(KT) | $(BUILD_ROOT)/bin
	$(INSTALL) -m 755 $(KT) $@

$(LEGACY_KRYON_CMD): $(KRYON_CMD) | $(BUILD_ROOT)/bin
	$(INSTALL) -m 755 $(KRYON_CMD) $@

$(BUILD_ROOT)/bin:
	mkdir -p $@
endif

all: $(LIB) $(K2C) $(K2IR) $(K2B) $(KT) $(KRYON_PREVIEW) $(KRYON_CMD)

tools: $(K2C) $(K2IR) $(K2B) $(KT) $(KRYON_PREVIEW) $(KRYON_CMD)

install: $(KT) $(KRYON_CMD)
	mkdir -p $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(KT) $(DESTDIR)$(BINDIR)/kt
	$(INSTALL) -m 755 $(KRYON_CMD) $(DESTDIR)$(BINDIR)/kryon

examples-run:
	@$(MAKE) -C examples run

clean:
	rm -rf $(BUILD_DIR)
	$(MAKE) -C examples web-clean

docs-site:
	rm -rf $(SITE_BUILD_DIR)
	mkdir -p $(SITE_BUILD_DIR)
	cp -R $(SITE_DIR)/. $(SITE_BUILD_DIR)/
	cp -R icons platforms language $(SITE_BUILD_DIR)/
	cp -R $(SITE_DIR)/cursors $(SITE_BUILD_DIR)/
	sh scripts/build-site-web-ide.sh $(SITE_BUILD_DIR)
	sh scripts/render-api-html.sh docs/API.md $(SITE_DIR)/api-template.html $(SITE_BUILD_DIR)/api.html
	rm -f $(SITE_BUILD_DIR)/api-template.html

test: kryon-compat-check kryon-boundary-check $(K2C) $(K2IR) $(K2B) $(KT) $(KSYNC_ACCOUNT_TEST) $(KSYNC_SYNC_TEST) $(TRANSITION_TEST) $(FILE_DIALOG_BACKEND_TEST) $(MARKDOWN_TEST) $(RAYLIB_COMPAT_TEST) $(UI_TK_TEST) $(PREVIEW_TEST) $(PLATFORM_THREAD_TEST) $(UI_TEXT_EDIT_TEST) $(UI_TREE_API_TEST) $(SCENE_TREE_TEST) $(SCENE_PROPERTY_TEST) $(ANIMATION_TEST) $(KIR_TEST) $(K2IR_TEST) $(KRB_WALK_TEST) $(KRB_MOUNT_TEST) $(KRY_TERM_TEST)
	sh tests/k2c_syntax_test.sh $(K2C)
	sh tests/kt_cli_test.sh $(KT)
	sh tests/krb_cartridge_test.sh $(K2B) $(KRB_WALK_TEST) .
	$(KRB_MOUNT_TEST)
	$(KRY_TERM_TEST)
	$(KSYNC_ACCOUNT_TEST)
	$(KSYNC_SYNC_TEST)
	$(TRANSITION_TEST)
	$(FILE_DIALOG_BACKEND_TEST)
	$(MARKDOWN_TEST)
	$(RAYLIB_COMPAT_TEST)
	$(UI_TK_TEST)
	$(PREVIEW_TEST)
	$(PLATFORM_THREAD_TEST)
	$(UI_TEXT_EDIT_TEST)
	$(UI_TREE_API_TEST)
	$(KIR_TEST)
	@cat $(K2IR_TEST)

bsd-check:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) test

kryon-compat: $(KRYON_COMPAT_HEADER) $(KRYON_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_WRAPPERS_C)

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

$(LIB): $(OBJS) | $(BUILD_DIR) $(KRYON_COMPAT_HEADER) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS) $(KRYON_PHYSICS_DEPS)
	rm -f $@
	$(AR) $(ARFLAGS) $@ $(OBJS)

K2C_SRCS := $(sort $(wildcard cmd/k2c/*.c))
K2C_HDRS := cmd/k2c/kc_internal.h cmd/k2c/kc_ast.h

$(K2C): $(K2C_SRCS) $(K2C_HDRS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -o $@ $(K2C_SRCS)

K2IR_SRCS := $(sort $(wildcard cmd/k2ir/*.c)) cmd/kir/kir.c cmd/kir/kir_parse.c
$(K2IR): $(K2IR_SRCS) cmd/kir/kir.h cmd/kir/kir_parse.h | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -Icmd/kir -o $@ $(K2IR_SRCS)

K2B_SRCS := $(sort $(wildcard cmd/k2b/*.c))
$(K2B): $(K2B_SRCS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS_BASE) -o $@ $(K2B_SRCS)

$(KT): cmd/kt/main.c | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS_BASE) -o $@ cmd/kt/main.c

$(KRYON_PREVIEW): cmd/kryon-preview/main.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ cmd/kryon-preview/main.c \
		-Wl,-export-dynamic \
		-Wl,--whole-archive $(LIB) -Wl,--no-whole-archive \
		$(RAYLIB_A) $(KRYON_PHYSICS_DEPS) $(RAY_LDLIBS) $(KRYON_LIBOQS_A) \
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

$(KSYNC_ACCOUNT_TEST): tests/ksync_account_test.c src/ksync/ksync_account.c include/ksync_account.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/ksync_account_test.c src/ksync/ksync_account.c \
		$(KRYON_LIBOQS_A) -lm -o $@

$(KSYNC_SYNC_TEST): tests/ksync_sync_test.c src/ksync/ksync_sync.c src/ksync/ksync_account.c include/ksync_sync.h include/ksync_account.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/ksync_sync_test.c src/ksync/ksync_sync.c src/ksync/ksync_account.c \
		$(KRYON_LIBOQS_A) -lm -o $@

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

$(RAYLIB_COMPAT_TEST): tests/raylib_compat_test.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/raylib_compat_test.c \
		$(LIB) $(RAYLIB_A) $(RAYLIB_COMPAT_LDLIBS) \
		-o $@

$(UI_TK_TEST): tests/ui_tk_test.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_tk_test.c \
		$(LIB) $(RAYLIB_A) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(PREVIEW_TEST): tests/preview_test.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/preview_test.c \
		$(LIB) $(RAYLIB_A) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(PLATFORM_THREAD_TEST): tests/platform_thread_test.c src/platform/platform_thread.c include/platform.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/platform_thread_test.c \
		src/platform/platform_thread.c -lpthread -o $@

$(UI_TEXT_EDIT_TEST): tests/ui_text_edit_test.c src/ui/ui_text_edit.c include/kryon.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_text_edit_test.c src/ui/ui_text_edit.c -o $@

$(UI_TREE_API_TEST): tests/ui_tree_api_test.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/ui_tree_api_test.c \
		$(LIB) $(RAYLIB_A) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(SCENE_TREE_TEST): tests/scene_tree_test.c $(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/scene_tree_test.c \
		$(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(SCENE_PROPERTY_TEST): tests/scene_property_test.c $(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/scene_property_test.c \
		$(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
		-o $@

$(ANIMATION_TEST): tests/animation_test.c $(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/animation_test.c \
		$(LIB) $(RAYLIB_A) $(KRYON_PHYSICS_DEPS) $(RAYLIB_COMPAT_LDLIBS) $(LDLIBS) \
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

$(KRY_TERM_TEST): tests/kry_term_test.c src/kry_std/kry_term.c include/kry_term.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/kry_term_test.c src/kry_std/kry_term.c -o $@

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/ui_icons.h
	sh scripts/embed-icons.sh "$(ICON_DIR)" $@

src/ui/ui_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) scripts/embed-assets.sh include/embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
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
