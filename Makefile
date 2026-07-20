CC ?= cc
AR ?= ar
BUILD_DIR ?= build
PREFIX ?= /usr/local
SITE_DIR ?= docs/site
SITE_BUILD_DIR ?= $(BUILD_DIR)/site
VERSION_FILE ?= include/kryon_version.h
KRYON_VERSION_STRING := $(shell sed -n 's/^#define KRYON_VERSION_STRING "\([^"]*\)".*/\1/p' $(VERSION_FILE) 2>/dev/null)
VERSION ?= $(if $(strip $(KRYON_VERSION_STRING)),$(KRYON_VERSION_STRING),$(shell git describe --tags --always --dirty 2>/dev/null || printf '%s' 0.0.0))
DIST_DIR ?= dist
STATIC_DIST_ROOT := $(BUILD_DIR)/dist/kryon-$(VERSION)-static
STATIC_DIST_ARCHIVE := $(DIST_DIR)/kryon-$(VERSION)-static.tar.gz
KC = $(BUILD_DIR)/bin/kc
KI = $(BUILD_DIR)/bin/ki
CFLAGS ?= -Wall -Wextra -O2
CPPFLAGS_BASE = -Iinclude -I$(KRYON_DIR)/vendor/clay
ICON_DIR ?= icons language payments platforms tiles pfp
ICON_FILES = $(foreach dir,$(ICON_DIR),$(wildcard $(dir)/*.png))
ICON_ASSETS_C = src/ui/ui_icon_assets.c
EMBED_ASSETS ?= themes fonts/noto
EMBED_ASSET_FILES = $(shell find $(EMBED_ASSETS) -type f 2>/dev/null)
EMBED_ASSETS_C = $(BUILD_DIR)/embedded_asset_data.c
KRYON_COMPAT_GENERATOR = tools/generate-kryon-compat.sh
KRYON_BOUNDARY_CHECK = tools/check-kryon-boundaries.sh
KRYON_COMPAT_SYMBOL_CHECK = tools/check-raylib-compat-symbols.sh
KRYON_COMPAT_HEADER = include/kryon_compat.generated.h
KRYON_BACKEND_RENAME_HEADER = $(BUILD_DIR)/generated/raylib_backend_rename.h
KRYON_RAYLIB_WRAPPERS_C = $(BUILD_DIR)/generated/kryon_raylib_wrappers.c
KRYON_RAYLIB_GENERATED_PUBLIC_HEADER ?= $(KRYON_COMPAT_HEADER)
KRYON_RAYLIB_BACKEND_RENAME_HEADER ?= $(KRYON_BACKEND_RENAME_HEADER)
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
CURL_CODEC_LDLIBS != pkg-config --libs libbrotlidec libbrotlicommon libzstd 2>/dev/null || true
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=1
APP_RAYLIB_CONFIG ?= $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
KRYON_STATIC_PACKAGE_EXTERNAL_LIBS ?= $(RAY_LDLIBS) $(KRYON_OPENSSL_SSL_LDLIB) $(KRYON_OPENSSL_CRYPTO_LDLIB) -lpthread -lm
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

SRCS += $(EMBED_ASSETS_C) $(KRYON_RAYLIB_WRAPPERS_C)

SYSTEM_THEME_PKG := $(shell if pkg-config --exists gtk+-3.0 2>/dev/null; then printf '%s' gtk+-3.0; fi)
ifneq ($(strip $(SYSTEM_THEME_PKG)),)
    CPPFLAGS += $(shell pkg-config --cflags $(SYSTEM_THEME_PKG)) -DSYSTEM_THEME_GTK
    LDLIBS += $(shell pkg-config --libs $(SYSTEM_THEME_PKG))
endif

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(filter src/%,$(SRCS))) \
	$(patsubst $(BUILD_DIR)/%.c,$(BUILD_DIR)/%.o,$(filter $(BUILD_DIR)/%,$(SRCS)))
LIB = libkryon.a
LYRA_ACCOUNT_TEST = $(BUILD_DIR)/tests/lyra_account_test
LYRA_SYNC_TEST = $(BUILD_DIR)/tests/lyra_sync_test
TRANSITION_TEST = $(BUILD_DIR)/tests/transition_test
FILE_DIALOG_BACKEND_TEST = $(BUILD_DIR)/tests/file_dialog_backend_test
MARKDOWN_TEST = $(BUILD_DIR)/tests/markdown_test
RAYLIB_COMPAT_TEST = $(BUILD_DIR)/tests/raylib_compat_test
UI_TK_TEST = $(BUILD_DIR)/tests/ui_tk_test
RAYLIB_COMPAT_LDLIBS ?= $(RAY_LDLIBS) -lpthread -lm $(if $(filter linux,$(KRYON_PLATFORM)),-ldl -lrt,)

.PHONY: all clean run tools examples-run font-assets docs-site test bsd-check kryon-compat kryon-compat-check kryon-boundary-check version release-check dist-static check-static-package install-static

all: $(LIB) $(KC) $(KI)

run: $(KI)
	@if [ -n "$(PROJECT)" ]; then \
		project_path=$$(cd "$(PROJECT)" && pwd); \
		KRYON_INSPECT=1 KRYON_PROJECT_ROOT="$$project_path" $(KI) "$$project_path"; \
	elif [ -n "$(ARGS)" ]; then \
		project_path=$$(cd "$(ARGS)" && pwd); \
		KRYON_INSPECT=1 KRYON_PROJECT_ROOT="$$project_path" $(KI) "$$project_path"; \
	elif [ -n "$(KRYON_PROJECT)" ]; then \
		project_path=$$(cd "$(KRYON_PROJECT)" && pwd); \
		KRYON_INSPECT=1 KRYON_PROJECT_ROOT="$$project_path" $(KI) "$$project_path"; \
	else \
		project_path=$$(pwd); \
		KRYON_INSPECT=1 KRYON_PROJECT_ROOT="$$project_path" $(KI); \
	fi

tools: $(KC) $(KI)

examples-run:
	@$(MAKE) -C examples run

clean:
	rm -rf $(BUILD_DIR) $(LIB)
	$(MAKE) -C examples web-clean

docs-site:
	rm -rf $(SITE_BUILD_DIR)
	mkdir -p $(SITE_BUILD_DIR)
	cp -R $(SITE_DIR)/. $(SITE_BUILD_DIR)/
	cp docs/API.md $(SITE_BUILD_DIR)/API.md
	cp -R icons platforms language $(SITE_BUILD_DIR)/
	$(MAKE) -C examples web EXAMPLES_WEB_SITE_DIR="$(abspath $(SITE_BUILD_DIR))/examples"

test: kryon-compat-check kryon-boundary-check $(LYRA_ACCOUNT_TEST) $(LYRA_SYNC_TEST) $(TRANSITION_TEST) $(FILE_DIALOG_BACKEND_TEST) $(MARKDOWN_TEST) $(RAYLIB_COMPAT_TEST) $(UI_TK_TEST)
	$(LYRA_ACCOUNT_TEST)
	$(LYRA_SYNC_TEST)
	$(TRANSITION_TEST)
	$(FILE_DIALOG_BACKEND_TEST)
	$(MARKDOWN_TEST)
	$(RAYLIB_COMPAT_TEST)
	$(UI_TK_TEST)

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

$(LIB): $(OBJS) | $(KRYON_COMPAT_HEADER) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

$(KC): cmd/kc/kc.c | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) -o $@ cmd/kc/kc.c

$(KI): cmd/ki/main.c $(LIB) $(RAYLIB_A) $(KRYON_LIBOQS_A) $(KRYON_CURL_A) $(KRYON_MARKDOWN_DEPS) | $(BUILD_DIR)/bin
	$(CC) $(CFLAGS) $(CPPFLAGS) -Isrc/ui $(RAY_CFLAGS) -o $@ \
		cmd/ki/main.c \
		-Wl,--whole-archive $(LIB) -Wl,--no-whole-archive \
		$(RAYLIB_A) $(KRYON_LIBOQS_A) $(KRYON_CURL_LDLIBS) \
		$(KRYON_MARKDOWN_LDLIBS) \
		-Wl,-export-dynamic $(RAY_LDLIBS) $(LDLIBS) \
		$(CURL_CODEC_LDLIBS) -lz -lpthread -lm

version:
	@printf '%s\n' '$(VERSION)'

release-check:
	@test -n '$(VERSION)'
	@grep -q '^## \[$(VERSION)\]' CHANGELOG.md

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

$(LYRA_ACCOUNT_TEST): tests/lyra_account_test.c src/lyra/lyra_account.c include/lyra_account.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/lyra_account_test.c src/lyra/lyra_account.c \
		$(KRYON_LIBOQS_A) -lm -o $@

$(LYRA_SYNC_TEST): tests/lyra_sync_test.c src/lyra/lyra_sync.c src/lyra/lyra_account.c include/lyra_sync.h include/lyra_account.h $(KRYON_LIBOQS_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAS_LIBOQS=1 $(KRYON_LIBOQS_INCLUDE) \
		tests/lyra_sync_test.c src/lyra/lyra_sync.c src/lyra/lyra_account.c \
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
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c -o $@

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

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/ui_icons.h
	sh scripts/embed-icons.sh "$(ICON_DIR)" $@

src/ui/ui_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) scripts/embed-assets.sh include/embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR) $(KRYON_LIBOQS_A) $(KRYON_CURL_PROTOCOL_CHECK) $(KRYON_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BUILD_DIR)/bin:
	mkdir -p $@

font-assets:
	@printf '%s\n' 'font assets are checked in or supplied by downstream apps'
