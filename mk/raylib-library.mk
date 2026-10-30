# raylib keeps its library sources and Makefile in src/ of the package.
RAYLIB_PACKAGE = $(eval RAYLIB_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path raylib $(if $(wildcard ziran.local.toml),,--locked)))$(RAYLIB_PACKAGE)
RAYLIB_SOURCE = $(addsuffix /src,$(RAYLIB_PACKAGE))
RAYLIB_BUILD ?= $(KRYON_DIR)/build/raylib-ziran
RAYLIB_A ?= $(RAYLIB_BUILD)/libraylib.a
RAYLIB_INPUTS = $(wildcard $(RAYLIB_SOURCE)/*.c $(RAYLIB_SOURCE)/*.h $(RAYLIB_SOURCE)/platforms/*.c $(RAYLIB_SOURCE)/Makefile)
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
else
RAYLIB_SDL_INCLUDE := $(shell pkg-config --variable=includedir sdl2)
RAYLIB_CFLAGS ?= $(shell pkg-config --cflags sdl2 libdrm gbm egl glesv2)
RAYLIB_LIBS ?= $(shell pkg-config --libs sdl2 libdrm gbm egl glesv2)
RAYLIB_PLATFORM ?= PLATFORM_DESKTOP_SDL
RAYLIB_GRAPHICS ?= GRAPHICS_API_OPENGL_ES2
RAYLIB_PLATFORM_LIBS := $(RAYLIB_LIBS) -ldl -lpthread
endif
RAYLIB_CC ?= $(CC)
RAYLIB_AR ?= $(AR)
RAYLIB_JOBS ?= 4

# Resolve the optional source package once, only when its archive is needed.
.SECONDEXPANSION:
$(RAYLIB_A): $$(RAYLIB_INPUTS)
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
