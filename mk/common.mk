CC ?= cc
AR ?= ar
WINDRES = windres
KRYON_MAKE_DIR ?= $(dir $(lastword $(MAKEFILE_LIST)))
ifeq ($(.DEFAULT_GOAL),)
.DEFAULT_GOAL := all
endif

APP_NAME ?= app
SRC ?= $(shell find src -type f -name '*.c' | LC_ALL=C sort)
CORE_DIR ?=
CORE_SRCS ?= $(if $(strip $(CORE_DIR)),$(wildcard $(CORE_DIR)/*.c),)
CORE_INCLUDE ?= $(if $(strip $(CORE_DIR)),-I$(CORE_DIR),)
CORE_A = $(if $(strip $(CORE_SRCS)),$(NATIVE_OBJ_DIR)/$(ARCH)/native/lib$(APP_NAME)-core.a,)

KRYON_DIR ?= vendor/kryon
RAYLIB_BUILD_DIR = $(NATIVE_OBJ_DIR)/$(ARCH)/native/raylib
RAYLIB_A = $(RAYLIB_BUILD_DIR)/libraylib.a
KRYON_ICON_ASSETS_C = $(KRYON_DIR)/src/ui/ui_icon_assets.c
KRYON_SRCS = $(filter-out $(KRYON_ICON_ASSETS_C),$(shell find $(KRYON_DIR)/src -type f -name '*.c' | LC_ALL=C sort)) $(KRYON_ICON_ASSETS_C) $(KRYON_RAYLIB_WRAPPERS_C)
KRYON_INCLUDE = -I$(KRYON_DIR)/include
KRYON_USE_SYSTEM_CURL ?= 1
ifeq ($(KRYON_USE_SYSTEM_CURL),1)
KRYON_CURL_CFLAGS ?= $(shell pkg-config --cflags libcurl 2>/dev/null)
KRYON_CURL_LDLIBS ?= $(shell pkg-config --libs libcurl 2>/dev/null)
endif
KRYON_NATIVE_DEPS ?=
KRYON_NATIVE_CFLAGS ?=
KRYON_NATIVE_LDLIBS ?=
KRYON_RAYLIB_MODULE_AUDIO ?= TRUE
KRYON_NATIVE_SUPPORT_FLAGS ?= -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1
KRYON_NATIVE_SYSTEM_LDLIBS ?= -lm -lpthread
ifneq ($(strip $(KRYON_CURL_LDLIBS)),)
KRYON_RUNTIME_ASSET_CFLAGS = -DHAS_LIBCURL=1 $(KRYON_CURL_CFLAGS)
KRYON_RUNTIME_ASSET_LDLIBS = $(KRYON_CURL_LDLIBS)
else
KRYON_RUNTIME_ASSET_CFLAGS =
KRYON_RUNTIME_ASSET_LDLIBS =
endif

ANDROID_DIR ?= droid
GRADLE ?= gradle
ANDROID_APP_ID ?= xyz.waozi.$(APP_NAME)
ANDROID_ACTIVITY ?= android.app.NativeActivity

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    PLATFORM = linux
    KRYON_NATIVE_SYSTEM_LDLIBS += -ldl -lrt
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
NATIVE_OBJ_DIR = $(BUILD_OBJ_DIR)/$(PLATFORM)
NATIVE_BIN_DIR = $(BUILD_BIN_DIR)/$(PLATFORM)
NATIVE_DIST_DIR = $(BUILD_DIST_DIR)/$(PLATFORM)
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
THEME_FILES = $(wildcard $(KRYON_DIR)/themes/*.ini)
IMAGE_FILES ?=
SOUND_FILES ?=
FONT_FILES ?= $(wildcard fonts/noto/*.ttf fonts/noto/*.otf)
EMBEDDED_ASSET_FILES = $(LOCALE_FILES) $(THEME_FILES) $(IMAGE_FILES) $(SOUND_FILES) $(FONT_FILES)
EMBEDDED_ASSETS_C = $(BUILD_OBJ_DIR)/$(APP_NAME)_embedded_assets.c
SRC += $(EMBEDDED_ASSETS_C)
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
RAY_RAYLIB_CONFIG ?= -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_CLIPBOARD_IMAGE=0 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 -DSUPPORT_FILEFORMAT_TTF=1
KRYON_MAKEFILES = $(KRYON_MAKE_DIR)common.mk $(KRYON_MAKE_DIR)raylib.mk $(KRYON_MAKE_DIR)native.mk $(KRYON_MAKE_DIR)windows.mk $(KRYON_MAKE_DIR)web.mk $(KRYON_MAKE_DIR)android.mk $(KRYON_MAKE_DIR)dist.mk $(KRYON_MAKE_DIR)package-freebsd.mk $(KRYON_MAKE_DIR)clean.mk
BUILD_MAKEFILES = Makefile $(KRYON_MAKEFILES)

$(EMBEDDED_ASSETS_C): $(EMBEDDED_ASSET_FILES) $(KRYON_DIR)/scripts/embed-assets.sh | $(BUILD_OBJ_DIR)
	sh $(KRYON_DIR)/scripts/embed-assets.sh $@ $(EMBEDDED_ASSET_FILES)

ifeq ($(UNAME_S),FreeBSD)
KRYON_RAYLIB_AUDIO_PERIOD_FRAMES ?= 128
KRYON_RAYLIB_AUDIO_PERIODS ?= 2
endif
KRYON_RAYLIB_AUDIO_PERIOD_CONFIG = $(if $(strip $(KRYON_RAYLIB_AUDIO_PERIOD_FRAMES)),-DAUDIO_DEVICE_PERIOD_SIZE_IN_FRAMES=$(KRYON_RAYLIB_AUDIO_PERIOD_FRAMES),)
KRYON_RAYLIB_AUDIO_PERIODS_CONFIG = $(if $(strip $(KRYON_RAYLIB_AUDIO_PERIODS)),-DAUDIO_DEVICE_PERIODS=$(KRYON_RAYLIB_AUDIO_PERIODS),)

APP_RAYLIB_CONFIG = $(filter-out -DSUPPORT_MODULE_RAUDIO=0 -DSUPPORT_FILEFORMAT_PNG=0 -DSUPPORT_FILEFORMAT_JPG=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0,$(RAY_RAYLIB_CONFIG)) -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 $(KRYON_RAYLIB_AUDIO_PERIOD_CONFIG) $(KRYON_RAYLIB_AUDIO_PERIODS_CONFIG)

ifeq ($(UNAME_S),Linux)
KRYON_NATIVE_FEATURE_DEFS ?= -D_DEFAULT_SOURCE -D_GNU_SOURCE
else
KRYON_NATIVE_FEATURE_DEFS ?=
endif

CFLAGS = -Wall -Wextra -std=c99 -Os $(KRYON_NATIVE_FEATURE_DEFS) -ffunction-sections -fdata-sections -DSUPPORT_FILEFORMAT_JPG=1 -DMINIZ_NO_ZLIB_COMPATIBLE_NAMES -DUI_EMBEDDED_ONLY=1 $(KRYON_RUNTIME_ASSET_CFLAGS)
LDFLAGS = -Wl,--gc-sections -s

.NOTPARALLEL:

ifeq ($(strip $(RAY_CFLAGS)),)
$(error RAY_CFLAGS is not set. Install pkg-config metadata for $(RAY_PKGS), or set RAY_CFLAGS explicitly)
endif
ifeq ($(strip $(RAY_LDLIBS)),)
$(error RAY_LDLIBS is not set. Install pkg-config metadata for $(RAY_PKGS), or set RAY_LDLIBS explicitly)
endif
ifeq ($(strip $(RAY_SDL_LDLIBS)),)
$(error RAY_SDL_LDLIBS is not set. Install SDL2 development files or set RAY_SDL_LDLIBS explicitly)
endif
ifeq ($(strip $(RAY_SDL_INCLUDE_DIR)),)
$(error RAY_SDL_INCLUDE_DIR is not set. Install SDL2 development files or set RAY_SDL_INCLUDE_DIR explicitly)
endif
ifeq ($(strip $(RAY_RAYLIB_CONFIG)),)
$(error RAY_RAYLIB_CONFIG is not set. Set RAY_RAYLIB_CONFIG explicitly)
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

$(NATIVE_OBJ_DIR): | $(BUILD_OBJ_DIR)
	mkdir -p $@

$(NATIVE_BIN_DIR): | $(BUILD_BIN_DIR)
	mkdir -p $@

$(NATIVE_DIST_DIR): | $(BUILD_DIST_DIR)
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
