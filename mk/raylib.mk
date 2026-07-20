ifndef KRYON_RAYLIB_MK_INCLUDED
KRYON_RAYLIB_MK_INCLUDED := 1

KRYON_DIR ?= vendor/kryon
RAYLIB_DIR ?= $(KRYON_DIR)/vendor/raylib/src

RAYLIB_SOURCES = $(shell if [ -d "$(RAYLIB_DIR)" ]; then find "$(RAYLIB_DIR)" -type f \( -name '*.c' -o -name '*.h' \); fi)
KRYON_RAYLIB_GENERATED_DIR ?= $(BUILD_DIR)/generated
KRYON_RAYLIB_GENERATED_PUBLIC_HEADER ?= $(KRYON_RAYLIB_GENERATED_DIR)/kryon_compat.generated.h
KRYON_RAYLIB_BACKEND_RENAME_HEADER ?= $(KRYON_RAYLIB_GENERATED_DIR)/raylib_backend_rename.h
KRYON_RAYLIB_WRAPPERS_C ?= $(KRYON_RAYLIB_GENERATED_DIR)/kryon_raylib_wrappers.c
KRYON_RAYLIB_PREPARE_SCRIPT ?= $(KRYON_DIR)/scripts/prepare-raylib-source.sh
KRYON_RAYLIB_BACKEND_RENAME_CFLAG = -include $(abspath $(KRYON_RAYLIB_BACKEND_RENAME_HEADER))

KRYON_RAYLIB_WEB_BASE_SRCS = rcore.c rshapes.c rtextures.c rtext.c raudio.c
KRYON_RAYLIB_WEB_UTILITY_SRCS = $(notdir $(wildcard $(RAYLIB_DIR)/utils.c) $(wildcard $(RAYLIB_DIR)/rutils.c))
KRYON_RAYLIB_WEB_SRCS = $(KRYON_RAYLIB_WEB_BASE_SRCS) $(KRYON_RAYLIB_WEB_UTILITY_SRCS)
KRYON_RAYLIB_WEB_OBJS = $(patsubst %.c,$(WEB_RAYLIB_BUILD_DIR)/%.o,$(KRYON_RAYLIB_WEB_SRCS))

.PHONY: kryon-raylib-check

kryon-raylib-check:
	@if [ ! -f "$(RAYLIB_DIR)/raylib.h" ]; then \
		echo "raylib not found at $(RAYLIB_DIR)"; \
		echo "Initialize Kryon submodules or set RAYLIB_DIR=/path/to/raylib/src"; \
		exit 1; \
	fi

$(KRYON_RAYLIB_GENERATED_PUBLIC_HEADER) $(KRYON_RAYLIB_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_WRAPPERS_C): $(KRYON_DIR)/tools/generate-kryon-compat.sh $(RAYLIB_DIR)/raylib.h
	sh $(KRYON_DIR)/tools/generate-kryon-compat.sh $(RAYLIB_DIR)/raylib.h \
		$(KRYON_RAYLIB_GENERATED_PUBLIC_HEADER) \
		$(KRYON_RAYLIB_BACKEND_RENAME_HEADER) \
		$(KRYON_RAYLIB_WRAPPERS_C)

KRYON_RAYLIB_MODULE_AUDIO ?= TRUE
KRYON_RAYLIB_MODULE_MODELS ?= TRUE
KRYON_RAYLIB_BUILD_OPT_FLAGS ?= -Os -ffunction-sections -fdata-sections

define KRYON_RAYLIB_DESKTOP_RULE
$(1): kryon-raylib-check $(RAYLIB_SOURCES) $(KRYON_RAYLIB_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_PREPARE_SCRIPT) $(BUILD_MAKEFILES)
	@set -e; \
	lock="$(2).lock"; \
	while ! mkdir "$$lock" 2>/dev/null; do sleep 1; done; \
	trap 'rmdir "$$lock"' 0 1 2 3 15; \
	rm -rf $(2); \
	mkdir -p $(2) $(3); \
	cp -R $(RAYLIB_DIR)/. $(2)/; \
	sh $(KRYON_RAYLIB_PREPARE_SCRIPT) $(2); \
	$(MAKE) -j1 -C $(2) \
		CC="$(4)" \
		AR="$(5)" \
		$(if $(strip $(6)),RANLIB="$(6)",) \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../$(notdir $(3)) \
		RAYLIB_MODULE_AUDIO=$(KRYON_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=$(KRYON_RAYLIB_MODULE_MODELS) \
		SDL_INCLUDE_PATH="$(7)" \
		SDL_LIBRARIES="$(8)" \
		CUSTOM_CFLAGS="$(KRYON_RAYLIB_BACKEND_RENAME_CFLAG) -DUSING_SDL2_PROJECT $(9) $(APP_RAYLIB_CONFIG) $(KRYON_RAYLIB_BUILD_OPT_FLAGS)"
endef

define KRYON_RAYLIB_WEB_RULE
$(1): kryon-raylib-check $(RAYLIB_SOURCES) $(KRYON_RAYLIB_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_PREPARE_SCRIPT) $(BUILD_MAKEFILES)
	@set -e; \
	lock="$(2).lock"; \
	while ! mkdir "$$lock" 2>/dev/null; do sleep 1; done; \
	trap 'rmdir "$$lock"' 0 1 2 3 15; \
	rm -rf $(2); \
	mkdir -p $(2) $(3); \
	cp -R $(RAYLIB_DIR)/. $(2)/; \
	sh $(KRYON_RAYLIB_PREPARE_SCRIPT) $(2); \
	$(MAKE) -j1 -C $(2) \
		PLATFORM=PLATFORM_WEB \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../$(notdir $(3)) \
		RAYLIB_MODULE_AUDIO=$(KRYON_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=$(KRYON_RAYLIB_MODULE_MODELS) \
		CC="$(4)" \
		AR="$(5)" \
		CUSTOM_CFLAGS="$(KRYON_RAYLIB_BACKEND_RENAME_CFLAG) $(APP_RAYLIB_CONFIG) $(KRYON_RAYLIB_BUILD_OPT_FLAGS)"
endef

define KRYON_RAYLIB_WINDOWS_RULE
$(1): kryon-raylib-check $(RAYLIB_SOURCES) $(KRYON_RAYLIB_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_PREPARE_SCRIPT) $(BUILD_MAKEFILES)
	@set -e; \
	lock="$(2).lock"; \
	while ! mkdir "$$lock" 2>/dev/null; do sleep 1; done; \
	trap 'rmdir "$$lock"' 0 1 2 3 15; \
	rm -rf $(2); \
	mkdir -p $(2) $(3); \
	cp -R $(RAYLIB_DIR)/. $(2)/; \
	sh $(KRYON_RAYLIB_PREPARE_SCRIPT) $(2); \
	$(MAKE) -j1 -C $(2) \
		OS=Windows_NT \
		PLATFORM=PLATFORM_DESKTOP_RGFW \
		GRAPHICS=GRAPHICS_API_OPENGL_11 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../$(notdir $(3)) \
		RAYLIB_MODULE_AUDIO=$(KRYON_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=$(KRYON_RAYLIB_MODULE_MODELS) \
		CC="$(4)" \
		AR="$(5)" \
		RANLIB="$(6)" \
		CUSTOM_CFLAGS="$(KRYON_RAYLIB_BACKEND_RENAME_CFLAG) $(APP_RAYLIB_CONFIG) $(KRYON_RAYLIB_BUILD_OPT_FLAGS)"
endef

define KRYON_RAYLIB_WINDOWS_PLATFORM_RULE
$(1): kryon-raylib-check $(RAYLIB_SOURCES) $(KRYON_RAYLIB_BACKEND_RENAME_HEADER) $(KRYON_RAYLIB_PREPARE_SCRIPT) $(BUILD_MAKEFILES)
	@set -e; \
	lock="$(2).lock"; \
	while ! mkdir "$$lock" 2>/dev/null; do sleep 1; done; \
	trap 'rmdir "$$lock"' 0 1 2 3 15; \
	rm -rf $(2); \
	mkdir -p $(2) $(3); \
	cp -R $(RAYLIB_DIR)/. $(2)/; \
	sh $(KRYON_RAYLIB_PREPARE_SCRIPT) $(2); \
	$(MAKE) -j1 -C $(2) \
		OS=Windows_NT \
		PLATFORM=$(7) \
		PLATFORM_OS=WINDOWS \
		GRAPHICS=$(8) \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../$(notdir $(3)) \
		RAYLIB_MODULE_AUDIO=$(KRYON_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=$(KRYON_RAYLIB_MODULE_MODELS) \
		CC="$(4)" \
		AR="$(5)" \
		RANLIB="$(6)" \
		CUSTOM_CFLAGS="$(KRYON_RAYLIB_BACKEND_RENAME_CFLAG) $(APP_RAYLIB_CONFIG) $(KRYON_RAYLIB_BUILD_OPT_FLAGS)"
endef

ifneq ($(strip $(RAYLIB_A)),)
$(eval $(call KRYON_RAYLIB_DESKTOP_RULE,$(RAYLIB_A),$(or $(RAYLIB_SOURCE_BUILD_DIR),$(dir $(RAYLIB_BUILD_DIR))raylib-src),$(RAYLIB_BUILD_DIR),$(CC),$(or $(AR),ar),,$(RAY_SDL_INCLUDE_DIR),$(RAY_SDL_LDLIBS),$(RAY_CFLAGS)))
endif

ifneq ($(strip $(WEB_RAYLIB_A)),)
$(eval $(call KRYON_RAYLIB_WEB_RULE,$(WEB_RAYLIB_A),$(or $(WEB_RAYLIB_SOURCE_BUILD_DIR),$(dir $(WEB_RAYLIB_BUILD_DIR))raylib-src),$(WEB_RAYLIB_BUILD_DIR),$(WEB_CC),$(WEB_AR)))
endif

ifneq ($(strip $(CLICK_RAYLIB_A)),)
$(eval $(call KRYON_RAYLIB_DESKTOP_RULE,$(CLICK_RAYLIB_A),$(or $(CLICK_RAYLIB_SOURCE_BUILD_DIR),$(dir $(CLICK_RAYLIB_BUILD_DIR))raylib-src),$(CLICK_RAYLIB_BUILD_DIR),$(AARCH64_CC),$(AARCH64_AR),$(AARCH64_RANLIB),$(AARCH64_RAY_SDL_INCLUDE_DIR),$(AARCH64_RAY_SDL_LDLIBS),$(AARCH64_RAY_CFLAGS)))
endif

ifneq ($(strip $(WIN64_RAYLIB_A)),)
$(eval $(call KRYON_RAYLIB_WINDOWS_RULE,$(WIN64_RAYLIB_A),$(or $(WIN64_RAYLIB_SOURCE_BUILD_DIR),$(dir $(WIN64_RAYLIB_BUILD_DIR))raylib-src),$(WIN64_RAYLIB_BUILD_DIR),$(WIN64_CC),$(WIN64_AR),$(WIN64_RANLIB)))
endif

ifneq ($(strip $(WIN32_RAYLIB_A)),)
$(eval $(call KRYON_RAYLIB_WINDOWS_RULE,$(WIN32_RAYLIB_A),$(or $(WIN32_RAYLIB_SOURCE_BUILD_DIR),$(dir $(WIN32_RAYLIB_BUILD_DIR))raylib-src),$(WIN32_RAYLIB_BUILD_DIR),$(WIN32_CC),$(WIN32_AR),$(WIN32_RANLIB)))
endif

endif
