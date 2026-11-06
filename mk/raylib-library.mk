# raylib keeps its library sources and Makefile in src/ of the package.
RAYLIB_PACKAGE = $(eval RAYLIB_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path raylib $(if $(wildcard ziran.local.toml),,--locked)))$(RAYLIB_PACKAGE)
RAYLIB_SOURCE = $(addsuffix /src,$(RAYLIB_PACKAGE))
RAYLIB_BUILD ?= $(KRYON_DIR)/build/raylib-ziran
RAYLIB_A ?= $(RAYLIB_BUILD)/libraylib.a
RAYLIB_INPUTS = $(wildcard $(RAYLIB_SOURCE)/*.c $(RAYLIB_SOURCE)/*.h $(RAYLIB_SOURCE)/platforms/*.c $(RAYLIB_SOURCE)/Makefile)
# Package-owned decoder recipes keep Kryon's existing archive paths.
WEBP_CODEC_PACKAGE = $(eval WEBP_CODEC_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path webp $(if $(wildcard ziran.local.toml),,--locked)))$(WEBP_CODEC_PACKAGE)
WEBP_PACKAGE = $(eval WEBP_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libwebp $(if $(wildcard ziran.local.toml),,--locked)))$(WEBP_PACKAGE)
WEBP_BUILD ?= $(KRYON_DIR)/build/webp-ziran
WEBP_A ?= $(WEBP_BUILD)/libkryonwebp.a
WEBP_INPUTS = $(KRYON_DIR)/cmake/WebP.cmake $(KRYON_DIR)/cmake/ImagePackages.cmake $(KRYON_DIR)/cmake/webp/CMakeLists.txt $(wildcard $(WEBP_CODEC_PACKAGE)/native/*.cmake $(WEBP_PACKAGE)/CMakeLists.txt $(WEBP_PACKAGE)/src/*/*.c $(WEBP_PACKAGE)/src/*/*.h)
AVIF_CODEC_PACKAGE = $(eval AVIF_CODEC_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path avif $(if $(wildcard ziran.local.toml),,--locked)))$(AVIF_CODEC_PACKAGE)
AVIF_PACKAGE = $(eval AVIF_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libavif $(if $(wildcard ziran.local.toml),,--locked)))$(AVIF_PACKAGE)
AOM_PACKAGE = $(eval AOM_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libaom $(if $(wildcard ziran.local.toml),,--locked)))$(AOM_PACKAGE)
DAV1D_PACKAGE = $(eval DAV1D_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path dav1d $(if $(wildcard ziran.local.toml),,--locked)))$(DAV1D_PACKAGE)
AVIF_BUILD ?= $(KRYON_DIR)/build/avif-ziran
AVIF_A ?= $(AVIF_BUILD)/avif/libavif.a
AOM_A ?= $(AVIF_BUILD)/aom/libaom.a
AVIF_SO ?= $(AVIF_BUILD)/libkryonavif.so
AVIF_INPUTS = $(KRYON_DIR)/cmake/AVIF.cmake $(KRYON_DIR)/cmake/ImagePackages.cmake $(KRYON_DIR)/cmake/avif/CMakeLists.txt $(wildcard $(AVIF_CODEC_PACKAGE)/native/*.cmake)
RAYLIB_ARCHIVES = $(RAYLIB_A) $(WEBP_A) $(AVIF_A) $(AOM_A)
RAYLIB_HOST_OS := $(shell uname -s)
ifeq ($(RAYLIB_HOST_OS),Darwin)
RAYLIB_HOST_DEFINES := __APPLE__ $(if $(filter x86_64,$(shell uname -m)),__x86_64__,__aarch64__)
RAYLIB_PLATFORM ?= PLATFORM_DESKTOP_GLFW
RAYLIB_GRAPHICS ?= GRAPHICS_API_OPENGL_33
RAYLIB_CFLAGS ?=
RAYLIB_LIBS ?= -framework OpenGL -framework Cocoa -framework IOKit -framework CoreAudio -framework CoreVideo -framework QuartzCore
RAYLIB_PLATFORM_LIBS := $(RAYLIB_LIBS)
else ifneq ($(filter MINGW% MSYS% CYGWIN%,$(RAYLIB_HOST_OS)),)
RAYLIB_PLATFORM ?= PLATFORM_DESKTOP_GLFW
RAYLIB_GRAPHICS ?= GRAPHICS_API_OPENGL_33
RAYLIB_CFLAGS ?=
RAYLIB_LIBS ?= -lopengl32 -lgdi32 -lwinmm
RAYLIB_PLATFORM_LIBS := $(RAYLIB_LIBS)
RAYLIB_HOST_DEFINES := _WIN32
# media_audio_raylib decodes Ogg Opus assets with the audio package's libopus;
# its MinGW build keeps the stack protector, which needs GCC's libssp.
RAYLIB_AUDIO_LIBS ?= -lopus -lssp -lm
else
RAYLIB_SDL_INCLUDE := $(shell pkg-config --variable=includedir sdl2)
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags sdl2 libdrm gbm egl glesv2)
RAYLIB_LIBS ?= $(shell pkg-config --libs sdl2 libdrm gbm egl glesv2)
RAYLIB_PLATFORM ?= PLATFORM_DESKTOP_SDL
RAYLIB_GRAPHICS ?= GRAPHICS_API_OPENGL_ES2
RAYLIB_PLATFORM_LIBS := $(RAYLIB_LIBS) -ldl -lpthread
# media_audio_raylib decodes Ogg Opus assets with the audio package's libopus.
RAYLIB_AUDIO_LIBS ?= -lopus -lm
endif
RAYLIB_CC ?= $(CC)
RAYLIB_AR ?= $(AR)
RAYLIB_JOBS ?= 4
WEBP_CFLAGS ?= -O2

# Resolve the optional source package once, only when its archive is needed.
.SECONDEXPANSION:
$(RAYLIB_A): $$(RAYLIB_INPUTS) | $(WEBP_A)
	@test -n "$(RAYLIB_SOURCE)" || { echo "Run ziran lock; Kryon needs the locked raylib source package" >&2; exit 1; }
	@test -f $(RAYLIB_SOURCE)/raylib.h || { echo "The locked raylib source package is incomplete; run ziran fetch" >&2; exit 1; }
	mkdir -p $(RAYLIB_BUILD)/source
	cp -R $(RAYLIB_SOURCE)/. $(RAYLIB_BUILD)/source/
	$(MAKE) -j$(RAYLIB_JOBS) -C $(RAYLIB_BUILD)/source \
		RAYLIB_SRC_PATH=. RAYLIB_RELEASE_PATH=.. \
		PLATFORM=$(RAYLIB_PLATFORM) GRAPHICS=$(RAYLIB_GRAPHICS) \
		CC="$(RAYLIB_CC)" AR="$(RAYLIB_AR)" \
		RAYLIB_LIBTYPE=STATIC RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=TRUE \
		SDL_INCLUDE_PATH=$(RAYLIB_SDL_INCLUDE) \
		CUSTOM_CFLAGS="$(if $(filter PLATFORM_DESKTOP_SDL,$(RAYLIB_PLATFORM)),-DUSING_SDL2_PROJECT) $(RAYLIB_CFLAGS) -O2 -ffunction-sections -fdata-sections"
	@test -f $@

# `command` runs cmake through the shell: GNU Make 4.3's own search stops at the
# cmake folder Emscripten puts on PATH and fails with Permission denied.
$(WEBP_A): $$(WEBP_INPUTS)
	@test -n "$(WEBP_PACKAGE)" && test -f "$(WEBP_PACKAGE)/src/demux/anim_decode.c" || { echo "Run ziran fetch; Kryon needs the locked libwebp source package" >&2; exit 1; }
	cmake -S $(KRYON_DIR)/cmake/webp -B $(WEBP_BUILD) -DWEBP_DIR="$(WEBP_PACKAGE)" \
		-DKRYON_WEBP_PACKAGE_DIR="$(WEBP_CODEC_PACKAGE)" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(RAYLIB_CC)" \
		-DCMAKE_C_FLAGS="$(WEBP_CFLAGS)" >/dev/null
	command cmake --build $(WEBP_BUILD) --target kryonwebp --parallel $(RAYLIB_JOBS)
	@test -f $@

$(AVIF_A): $$(AVIF_INPUTS)
	cmake -S $(KRYON_DIR)/cmake/avif -B $(AVIF_BUILD) -DAVIF_DIR="$(AVIF_PACKAGE)" -DAOM_DIR="$(AOM_PACKAGE)" \
		-DDAV1D_DIR="$(DAV1D_PACKAGE)" -DKRYON_AVIF_PACKAGE_DIR="$(AVIF_CODEC_PACKAGE)" \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(RAYLIB_CC)"
	command cmake --build $(AVIF_BUILD) --target avif_static $(if $(filter Linux,$(RAYLIB_HOST_OS)),kryonavif) --parallel $(RAYLIB_JOBS)
	@test -f $@

$(AOM_A) $(AVIF_SO): $(AVIF_A)
	@test -f $@
