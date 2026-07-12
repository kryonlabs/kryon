CC ?= cc
AR ?= ar
BUILD_DIR ?= build
PREFIX ?= /usr/local
SITE_DIR ?= docs/site
SITE_BUILD_DIR ?= $(BUILD_DIR)/site
VERSION ?= $(shell git describe --tags --always --dirty 2>/dev/null || printf '%s' 0.0.0)
DIST_DIR ?= dist
STATIC_DIST_ROOT := $(BUILD_DIR)/dist/flint-$(VERSION)-static
STATIC_DIST_ARCHIVE := $(DIST_DIR)/flint-$(VERSION)-static.tar.gz
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
FLINT_COMPAT_SYMBOL_CHECK = tools/check-raylib-compat-symbols.sh
FLINT_COMPAT_HEADER = include/flint_compat.generated.h
FLINT_BACKEND_RENAME_HEADER = $(BUILD_DIR)/generated/raylib_backend_rename.h
FLINT_RAYLIB_WRAPPERS_C = $(BUILD_DIR)/generated/flint_raylib_wrappers.c
FLINT_RAYLIB_GENERATED_PUBLIC_HEADER ?= $(FLINT_COMPAT_HEADER)
FLINT_RAYLIB_BACKEND_RENAME_HEADER ?= $(FLINT_BACKEND_RENAME_HEADER)
FLINT_FONT_OUT ?= assets/fonts/locales
FLINT_FONT_OUTPUTS = $(FLINT_FONT_OUT).png $(FLINT_FONT_OUT).dat
FLINT_FONT_LOCALES ?= locales/*.txt
FLINT_FONT_LOCALE_FILES = $(wildcard $(FLINT_FONT_LOCALES))
FLINT_FONT_FALLBACK_LOCALES = tools/otfchop/locales/en.txt
FLINT_FONT_SOURCE ?= tools/otfchop/unifont-17.0.04.otf
FLINT_OTFCHOP ?= tools/otfchop/otfchop
UNAME_S := $(shell uname -s 2>/dev/null)
UNAME_M := $(shell uname -m 2>/dev/null)
ifeq ($(UNAME_M),amd64)
    FLINT_ARCH := x86_64
else
    FLINT_ARCH := $(UNAME_M)
endif
ifeq ($(UNAME_S),Linux)
    FLINT_PLATFORM := linux
else ifeq ($(UNAME_S),FreeBSD)
    FLINT_PLATFORM := freebsd
else ifeq ($(UNAME_S),Darwin)
    FLINT_PLATFORM := macos
else
    FLINT_PLATFORM := $(UNAME_S)
endif

RAYLIB_DIR ?= $(FLINT_DIR)/vendor/raylib/src
RAYLIB_BUILD_DIR ?= $(BUILD_DIR)/raylib
RAYLIB_A ?= $(RAYLIB_BUILD_DIR)/libraylib.a
RAY_PKGS ?= sdl2 libdrm gbm egl glesv2
RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_CFLAGS ?= $(strip $(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS))
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=1
APP_RAYLIB_CONFIG ?= $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
FLINT_STATIC_PACKAGE_EXTERNAL_LIBS ?= $(RAY_LDLIBS) $(FLINT_OPENSSL_SSL_LDLIB) $(FLINT_OPENSSL_CRYPTO_LDLIB) -lpthread -lm
FLINT_STATIC_PACKAGE_LIBS ?= -lflint -lraylib -loqs -lcurl -lcmark-gfm-extensions -lcmark-gfm $(FLINT_STATIC_PACKAGE_EXTERNAL_LIBS)
FLINT_STATIC_PACKAGE_CFLAGS ?= -I$${includedir} -DHAS_LIBOQS=1 -DHAS_LIBCURL=1 -DCURL_STATICLIB -DFLINT_HAS_CMARK_GFM=1

# Check if we're in nix-shell and use its flags
ifneq ($(NIX_CFLAGS_COMPILE),)
    CPPFLAGS_BASE += $(NIX_CFLAGS_COMPILE)
endif

CPPFLAGS += $(CPPFLAGS_BASE)
ARFLAGS ?= rcs
FLINT_DIR ?= .
FLINT_VENDOR_BUILD_DIR ?= $(BUILD_DIR)/vendor
include mk/vendor.mk
include mk/raylib.mk

CPPFLAGS += -DHAS_LIBOQS=1 $(FLINT_LIBOQS_INCLUDE) \
	-DHAS_LIBCURL=1 $(FLINT_CURL_CFLAGS) \
	$(FLINT_MARKDOWN_CFLAGS)

SRCS := $(shell find src -type f -name '*.c' | LC_ALL=C sort)

SRCS += $(EMBED_ASSETS_C) $(FLINT_RAYLIB_WRAPPERS_C)

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
MARKDOWN_TEST = $(BUILD_DIR)/tests/markdown_test
RAYLIB_COMPAT_TEST = $(BUILD_DIR)/tests/raylib_compat_test
RAYLIB_COMPAT_LDLIBS ?= $(RAY_LDLIBS) -lpthread -lm $(if $(filter linux,$(FLINT_PLATFORM)),-ldl -lrt,)

.PHONY: all clean run font-assets docs-site test bsd-check flint-compat flint-compat-check flint-boundary-check dist-static check-static-package install-static

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

test: flint-compat-check flint-boundary-check $(LYRA_ACCOUNT_TEST) $(LYRA_SYNC_TEST) $(TRANSITION_TEST) $(FILE_DIALOG_BACKEND_TEST) $(MARKDOWN_TEST) $(RAYLIB_COMPAT_TEST)
	$(LYRA_ACCOUNT_TEST)
	$(LYRA_SYNC_TEST)
	$(TRANSITION_TEST)
	$(FILE_DIALOG_BACKEND_TEST)
	$(MARKDOWN_TEST)
	$(RAYLIB_COMPAT_TEST)

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
	sh $(FLINT_COMPAT_SYMBOL_CHECK) vendor/raylib/src/raylib.h \
		$(BUILD_DIR)/check/raylib_backend_rename.h \
		$(BUILD_DIR)/check/flint_raylib_wrappers.c

flint-boundary-check:
	sh $(FLINT_BOUNDARY_CHECK) .

$(LIB): $(OBJS) | $(FLINT_COMPAT_HEADER) $(FLINT_LIBOQS_A) $(FLINT_CURL_PROTOCOL_CHECK) $(FLINT_MARKDOWN_DEPS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

dist-static: $(STATIC_DIST_ARCHIVE)

check-static-package: $(STATIC_DIST_ARCHIVE)
	sh scripts/check-static-package.sh $(STATIC_DIST_ARCHIVE)

install-static: $(STATIC_DIST_ARCHIVE)
	mkdir -p $(DESTDIR)$(PREFIX)
	tar -xzf $(STATIC_DIST_ARCHIVE) -C $(DESTDIR)$(PREFIX) --strip-components=1

$(STATIC_DIST_ARCHIVE): $(LIB) $(RAYLIB_A) $(FLINT_LIBOQS_A) $(FLINT_CURL_A) $(FLINT_MARKDOWN_DEPS) README.md LICENSE THIRD_PARTY_NOTICES.md docs/API.md examples/package/minimal.c examples/package/markdown.c scripts/check-static-package.sh
	rm -rf $(STATIC_DIST_ROOT)
	mkdir -p $(STATIC_DIST_ROOT)/include $(STATIC_DIST_ROOT)/lib $(STATIC_DIST_ROOT)/lib/pkgconfig $(STATIC_DIST_ROOT)/lib/cmake/flint $(STATIC_DIST_ROOT)/share/doc/flint $(STATIC_DIST_ROOT)/share/licenses/flint $(STATIC_DIST_ROOT)/examples $(DIST_DIR)
	cp -R include/. $(STATIC_DIST_ROOT)/include/
	cp $(LIB) $(RAYLIB_A) $(FLINT_LIBOQS_A) $(FLINT_CURL_A) $(FLINT_MARKDOWN_DEPS) $(STATIC_DIST_ROOT)/lib/
	cp README.md $(STATIC_DIST_ROOT)/
	cp LICENSE $(STATIC_DIST_ROOT)/
	cp LICENSE THIRD_PARTY_NOTICES.md $(STATIC_DIST_ROOT)/share/licenses/flint/
	cp docs/API.md $(STATIC_DIST_ROOT)/share/doc/flint/
	cp examples/package/*.c $(STATIC_DIST_ROOT)/examples/
	git submodule status > $(STATIC_DIST_ROOT)/SUBMODULES.txt
	printf '%s\n' '$(VERSION)' > $(STATIC_DIST_ROOT)/VERSION
	printf '%s\n' \
		'{' \
		'  "name": "flint",' \
		'  "version": "$(VERSION)",' \
		'  "target": "$(FLINT_PLATFORM)-$(FLINT_ARCH)",' \
		'  "compiler": "$(CC)",' \
		'  "static_libraries": ["libflint.a", "libraylib.a", "liboqs.a", "libcurl.a", "libcmark-gfm-extensions.a", "libcmark-gfm.a"],' \
		'  "external_libraries": "$(FLINT_STATIC_PACKAGE_EXTERNAL_LIBS)",' \
		'  "pkg_config": "lib/pkgconfig/flint.pc"' \
		'}' > $(STATIC_DIST_ROOT)/manifest.json
	printf '%s\n' \
		'prefix=$${pcfiledir}/../..' \
		'exec_prefix=$${prefix}' \
		'libdir=$${prefix}/lib' \
		'includedir=$${prefix}/include' \
		'' \
		'Name: Flint' \
		'Description: Flint C support library for raylib-style applications' \
		'Version: $(VERSION)' \
		'Cflags: $(FLINT_STATIC_PACKAGE_CFLAGS)' \
		'Libs: -L$${libdir} $(FLINT_STATIC_PACKAGE_LIBS)' \
		> $(STATIC_DIST_ROOT)/lib/pkgconfig/flint.pc
	printf '%s\n' \
		'include(CMakeFindDependencyMacro)' \
		'get_filename_component(FLINT_PACKAGE_PREFIX "$${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)' \
		'add_library(Flint::flint STATIC IMPORTED)' \
		'set_target_properties(Flint::flint PROPERTIES IMPORTED_LOCATION "$${FLINT_PACKAGE_PREFIX}/lib/libflint.a" INTERFACE_INCLUDE_DIRECTORIES "$${FLINT_PACKAGE_PREFIX}/include" INTERFACE_COMPILE_DEFINITIONS "HAS_LIBOQS=1;HAS_LIBCURL=1;CURL_STATICLIB;FLINT_HAS_CMARK_GFM=1")' \
		'add_library(Flint::raylib STATIC IMPORTED)' \
		'set_target_properties(Flint::raylib PROPERTIES IMPORTED_LOCATION "$${FLINT_PACKAGE_PREFIX}/lib/libraylib.a")' \
		'set(Flint_PACKAGE_LIBS "$${FLINT_PACKAGE_PREFIX}/lib/liboqs.a" "$${FLINT_PACKAGE_PREFIX}/lib/libcurl.a" "$${FLINT_PACKAGE_PREFIX}/lib/libcmark-gfm-extensions.a" "$${FLINT_PACKAGE_PREFIX}/lib/libcmark-gfm.a")' \
		'set_target_properties(Flint::flint PROPERTIES INTERFACE_LINK_LIBRARIES "Flint::raylib;$${Flint_PACKAGE_LIBS}")' \
		'set(Flint_LIBRARIES Flint::flint Flint::raylib $${Flint_PACKAGE_LIBS})' \
		> $(STATIC_DIST_ROOT)/lib/cmake/flint/FlintConfig.cmake
	tar -C $(BUILD_DIR)/dist -czf $@ flint-$(VERSION)-static

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

$(MARKDOWN_TEST): tests/markdown_test.c src/markdown.c include/markdown.h $(FLINT_MARKDOWN_DEPS) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(FLINT_MARKDOWN_CFLAGS) \
		tests/markdown_test.c src/markdown.c \
		$(FLINT_MARKDOWN_LDLIBS) -o $@

$(FILE_DIALOG_BACKEND_TEST): tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c include/file_dialog.h | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/file_dialog_backend_test.c src/file_dialog/file_dialog.c -o $@

$(RAYLIB_COMPAT_TEST): tests/raylib_compat_test.c $(LIB) $(RAYLIB_A) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/raylib_compat_test.c \
		$(LIB) $(RAYLIB_A) $(RAYLIB_COMPAT_LDLIBS) \
		-o $@

$(ICON_ASSETS_C): $(ICON_FILES) scripts/embed-icons.sh include/ui_icons.h
	sh scripts/embed-icons.sh "$(ICON_DIR)" $@

src/ui/ui_icon_names.c: $(ICON_FILES) scripts/embed-icons.sh include/ui_icon_types.h
	@$(MAKE) --quiet $(ICON_ASSETS_C)

$(EMBED_ASSETS_C): $(EMBED_ASSET_FILES) $(FLINT_FONT_OUTPUTS) scripts/embed-assets.sh include/embedded_assets.h | $(BUILD_DIR)
	sh scripts/embed-assets.sh $@ $(EMBED_ASSETS) $(FLINT_FONT_OUTPUTS)

$(BUILD_DIR)/%.o: src/%.c | $(BUILD_DIR) $(FLINT_LIBOQS_A) $(FLINT_CURL_PROTOCOL_CHECK) $(FLINT_MARKDOWN_DEPS)
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(BUILD_DIR)/%.c | $(BUILD_DIR) $(FLINT_LIBOQS_A) $(FLINT_CURL_PROTOCOL_CHECK) $(FLINT_MARKDOWN_DEPS)
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
