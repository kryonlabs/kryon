CC ?= cc
AR ?= ar
WINDRES = windres

APP_NAME ?= app
SRC ?= $(wildcard src/*.c) $(wildcard src/*/*.c)
CORE_DIR ?=
CORE_SRCS ?= $(if $(strip $(CORE_DIR)),$(wildcard $(CORE_DIR)/*.c),)
CORE_INCLUDE ?= $(if $(strip $(CORE_DIR)),-I$(CORE_DIR),)
CORE_A = $(if $(strip $(CORE_SRCS)),$(LINUX_OBJ_DIR)/$(ARCH)/native/lib$(APP_NAME)-core.a,)

FLINT_DIR ?= vendor/flint
RAYLIB_BUILD_DIR = $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib
RAYLIB_A = $(RAYLIB_BUILD_DIR)/libraylib.a
FLINT_ICON_ASSETS_C = $(FLINT_DIR)/src/ui_icon_assets.c
FLINT_SRCS = $(filter-out $(FLINT_ICON_ASSETS_C),$(wildcard $(FLINT_DIR)/src/*.c) $(wildcard $(FLINT_DIR)/src/ui/*.c)) $(FLINT_ICON_ASSETS_C)
FLINT_INCLUDE = -I$(FLINT_DIR)/include
FLINT_USE_SYSTEM_CURL ?= 1
ifeq ($(FLINT_USE_SYSTEM_CURL),1)
FLINT_CURL_CFLAGS ?= $(shell pkg-config --cflags libcurl 2>/dev/null)
FLINT_CURL_LDLIBS ?= $(shell pkg-config --libs libcurl 2>/dev/null)
endif
FLINT_NATIVE_DEPS ?=
FLINT_NATIVE_CFLAGS ?=
FLINT_NATIVE_LDLIBS ?=
FLINT_RAYLIB_MODULE_AUDIO ?= TRUE
FLINT_NATIVE_SUPPORT_FLAGS ?= -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
FLINT_NATIVE_SYSTEM_LDLIBS ?= -lm -lpthread
ifneq ($(strip $(FLINT_CURL_LDLIBS)),)
FLINT_RUNTIME_ASSET_CFLAGS = -DHAS_LIBCURL=1 $(FLINT_CURL_CFLAGS)
FLINT_RUNTIME_ASSET_LDLIBS = $(FLINT_CURL_LDLIBS)
else
FLINT_RUNTIME_ASSET_CFLAGS =
FLINT_RUNTIME_ASSET_LDLIBS =
endif

ANDROID_DIR ?= droid
GRADLE ?= gradle
ANDROID_APP_ID ?= xyz.waozi.$(APP_NAME)
ANDROID_ACTIVITY ?= android.app.NativeActivity

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    FLINT_NATIVE_SYSTEM_LDLIBS += -ldl -lrt
else ifeq ($(UNAME_S),Darwin)
    PLATFORM = macos
else ifeq ($(UNAME_S),FreeBSD)
    PLATFORM = freebsd
else
    PLATFORM = unknown
endif

ifeq ($(UNAME_M),x86_64)
    ARCH = x86_64
else ifeq ($(UNAME_M),amd64)
    ARCH = x86_64
else ifeq ($(UNAME_M),aarch64)
    ARCH = aarch64
else ifeq ($(UNAME_M),armv7l)
    ARCH = arm
else
    ARCH = $(UNAME_M)
endif

BUILD_DIR = build
BUILD_WORK_DIR = $(BUILD_DIR)/work
BUILD_OBJ_DIR = $(BUILD_DIR)/obj
BUILD_BIN_DIR = $(BUILD_DIR)/bin
BUILD_DIST_DIR = $(BUILD_DIR)/dist
LINUX_OBJ_DIR = $(BUILD_OBJ_DIR)/linux
LINUX_BIN_DIR = $(BUILD_BIN_DIR)/linux
LINUX_DIST_DIR = $(BUILD_DIST_DIR)/linux
WINDOWS_OBJ_DIR = $(BUILD_OBJ_DIR)/windows
WINDOWS_BIN_DIR = $(BUILD_BIN_DIR)/windows
WINDOWS_DIST_DIR = $(BUILD_DIST_DIR)/windows
ANDROID_BUILD_DIR = $(BUILD_DIR)/android
ANDROID_DIST_DIR = $(BUILD_DIST_DIR)/android
WEB_WORK_DIR = $(BUILD_WORK_DIR)/web
WEB_BUILD_DIR = $(WEB_WORK_DIR)
WEB_DIST_DIR = $(BUILD_DIST_DIR)/web
WEB_ZIP = $(BUILD_DIST_DIR)/$(APP_NAME)-web.zip
LINUX_ARCHES ?= x86_64 aarch64
ANDROID_DIST ?= release

INSTALL_DIR ?= $(HOME)/.local/share/$(APP_NAME)
BIN_DIR ?= $(HOME)/bin
TARBALL = $(LINUX_DIST_DIR)/$(APP_NAME)-linux.tar.gz

LOCALE_FILES = $(wildcard locales/*.txt)
THEME_FILES = $(wildcard $(FLINT_DIR)/themes/*.ini)
IMAGE_FILES ?=
SOUND_FILES ?=
FONT_OUTPUTS ?= assets/fonts/locales.png assets/fonts/locales.dat
FONT_FILES = $(FONT_OUTPUTS)
EMBEDDED_ASSET_FILES = $(LOCALE_FILES) $(THEME_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_FILES)
EMBEDDED_ASSETS_C = $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
SRC += $(EMBEDDED_ASSETS_C)
FONT_TOOL ?= $(FLINT_DIR)/tools/otfchop/otfchop
FONT_SOURCE ?= $(FLINT_DIR)/tools/otfchop/unifont-17.0.04.otf
FONT_INPUTS ?= $(LOCALE_FILES)
APP_INCLUDE ?= -Isrc -Isrc/android
RUNTIME_DIRS ?= assets locales themes
RAY_PKGS ?= sdl2 libdrm gbm egl glesv2
RAY_SDL_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null)
RAY_SDL_LDLIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null)
RAY_GL_CFLAGS ?= $(shell pkg-config --cflags libdrm gbm egl glesv2 2>/dev/null)
RAY_GL_LDLIBS ?= $(shell pkg-config --libs libdrm gbm egl glesv2 2>/dev/null)
RAY_CFLAGS ?= $(strip $(RAY_SDL_CFLAGS) $(RAY_GL_CFLAGS))
RAY_LDLIBS ?= $(strip $(RAY_SDL_LDLIBS) $(RAY_GL_LDLIBS))
RAY_SDL_INCLUDE_DIR ?= $(shell pkg-config --variable=includedir sdl2 2>/dev/null | sed 's,/SDL2$$,,')
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=0
FLINT_MAKEFILES = $(FLINT_MAKE_DIR)project.mk $(FLINT_MAKE_DIR)common.mk $(FLINT_MAKE_DIR)raylib.mk $(FLINT_MAKE_DIR)native.mk $(FLINT_MAKE_DIR)windows.mk $(FLINT_MAKE_DIR)web.mk $(FLINT_MAKE_DIR)android.mk $(FLINT_MAKE_DIR)dist.mk $(FLINT_MAKE_DIR)clean.mk
BUILD_MAKEFILES = Makefile $(FLINT_PROJECT) $(FLINT_MAKEFILES)
ifneq ($(strip $(FLINT_PROJECT_VARS)),)
BUILD_MAKEFILES += $(FLINT_PROJECT_VARS)
endif

assets/fonts:
	mkdir -p $@

$(FONT_OUTPUTS): $(FONT_INPUTS) $(FONT_TOOL) | assets/fonts
	$(FONT_TOOL) $(FONT_SOURCE) $(FONT_INPUTS) $(basename $(firstword $(FONT_OUTPUTS)))

$(FONT_TOOL): $(FLINT_DIR)/tools/otfchop/otfchop.c $(FLINT_DIR)/tools/otfchop/stb_truetype.h $(FLINT_DIR)/tools/otfchop/stb_image_write.h
	$(MAKE) -C $(FLINT_DIR)/tools/otfchop otfchop

$(EMBEDDED_ASSETS_C): $(EMBEDDED_ASSET_FILES) $(FLINT_DIR)/scripts/embed-assets.sh | $(BUILD_OBJ_DIR)
	sh $(FLINT_DIR)/scripts/embed-assets.sh $@ $(EMBEDDED_ASSET_FILES)

ifeq ($(UNAME_S),FreeBSD)
FLINT_RAYLIB_AUDIO_PERIOD_FRAMES ?= 128
FLINT_RAYLIB_AUDIO_PERIODS ?= 2
endif
FLINT_RAYLIB_AUDIO_PERIOD_CONFIG = $(if $(strip $(FLINT_RAYLIB_AUDIO_PERIOD_FRAMES)),-DAUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES=$(FLINT_RAYLIB_AUDIO_PERIOD_FRAMES),)
FLINT_RAYLIB_AUDIO_PERIODS_CONFIG = $(if $(strip $(FLINT_RAYLIB_AUDIO_PERIODS)),-DAUDIO_DEVICE_PERIODS=$(FLINT_RAYLIB_AUDIO_PERIODS),)

APP_RAYLIB_CONFIG = $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 $(FLINT_RAYLIB_AUDIO_PERIOD_CONFIG) $(FLINT_RAYLIB_AUDIO_PERIODS_CONFIG)

CFLAGS = -Wall -Wextra -std=c99 -Os -D_DEFAULT_SOURCE -D_GNU_SOURCE -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DUI_EMBEDDED_ONLY=1 $(FLINT_RUNTIME_ASSET_CFLAGS)
LDFLAGS = -Wl,--gc-sections -s

.NOTPARALLEL:

ifeq ($(strip $(RAY_CFLAGS)),)
$(error RAY_CFLAGS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_LDLIBS)),)
$(error RAY_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_LDLIBS)),)
$(error RAY_SDL_LDLIBS is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_SDL_INCLUDE_DIR)),)
$(error RAY_SDL_INCLUDE_DIR is not set. Enter the ray flake shell with 'nix develop')
endif
ifeq ($(strip $(RAY_RAYLIB_CONFIG)),)
$(error RAY_RAYLIB_CONFIG is not set. Enter the ray flake shell with 'nix develop')
endif

build:
	mkdir -p $(BUILD_DIR)

$(BUILD_OBJ_DIR):
	mkdir -p $@

$(BUILD_BIN_DIR):
	mkdir -p $@

$(BUILD_DIST_DIR):
	mkdir -p $@

$(BUILD_WORK_DIR):
	mkdir -p $@

$(LINUX_OBJ_DIR): | $(BUILD_OBJ_DIR)
	mkdir -p $@

$(LINUX_BIN_DIR): | $(BUILD_BIN_DIR)
	mkdir -p $@

$(LINUX_DIST_DIR): | $(BUILD_DIST_DIR)
	mkdir -p $@

$(WINDOWS_OBJ_DIR): | $(BUILD_OBJ_DIR)
	mkdir -p $@

$(WINDOWS_BIN_DIR): | $(BUILD_BIN_DIR)
	mkdir -p $@

$(WINDOWS_DIST_DIR): | $(BUILD_DIST_DIR)
	mkdir -p $@

$(ANDROID_DIST_DIR): | $(BUILD_DIST_DIR)
	mkdir -p $@

$(ANDROID_BUILD_DIR): | build
	mkdir -p $@

$(WEB_BUILD_DIR): | $(BUILD_WORK_DIR)
	mkdir -p $@

$(WEB_DIST_DIR): | $(BUILD_DIST_DIR)
	mkdir -p $@

$(RAYLIB_BUILD_DIR): | build
	mkdir -p $@
