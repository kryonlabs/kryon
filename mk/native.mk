BINARY_NAME = $(APP_NAME)-$(PLATFORM)-$(ARCH)
TARGET = $(LINUX_BIN_DIR)/$(BINARY_NAME)

all: native

native: $(TARGET)

$(RAYLIB_A): $(RAYLIB_SOURCES) $(BUILD_MAKEFILES) | $(RAYLIB_BUILD_DIR)
	@rm -rf $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src
	@mkdir -p $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src
	cp -R $(RAYLIB_DIR)/. $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src/
	$(MAKE) -j1 -C $(LINUX_OBJ_DIR)/$(ARCH)/native/raylib-src \
		CC="$(CC)" \
		AR="$(AR)" \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=$(FLINT_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAY_CFLAGS) $(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"

ifneq ($(strip $(CORE_SRCS)),)
CORE_OBJS = $(patsubst %.c,$(LINUX_OBJ_DIR)/$(ARCH)/native/core/%.o,$(CORE_SRCS))

$(LINUX_OBJ_DIR)/$(ARCH)/native/core/%.o: %.c | $(LINUX_OBJ_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(CORE_INCLUDE) -c $< -o $@

$(CORE_A): $(CORE_OBJS) | $(LINUX_OBJ_DIR)
	ar rcs $@ $(CORE_OBJS)
endif

$(TARGET): $(SRC) $(FLINT_SRCS) $(FONT_FILES) $(EMBEDDED_ASSETS_C) $(RAYLIB_A) $(CORE_A) $(FLINT_NATIVE_DEPS) | $(LINUX_BIN_DIR)
	$(CC) $(CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(CORE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		$(FLINT_NATIVE_CFLAGS) \
		$(RAY_CFLAGS) \
		$(FLINT_NATIVE_SUPPORT_FLAGS) \
		-o $@ \
		$(SRC) \
		$(FLINT_SRCS) \
		$(CORE_A) \
		$(RAYLIB_A) \
		$(RAY_LDLIBS) \
		$(FLINT_RUNTIME_ASSET_LDLIBS) \
		$(FLINT_NATIVE_LDLIBS) \
		$(FLINT_NATIVE_SYSTEM_LDLIBS) \
		$(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

linux: $(LINUX_ARCHES:%=linux-%)

linux-x86_64: | $(LINUX_BIN_DIR) $(LINUX_OBJ_DIR)
	$(MAKE) build-linux-arch \
		ARCH_NAME=x86_64 \
		LINUX_CC="$(CC)" \
		LINUX_AR=ar \
		LINUX_RAY_CFLAGS="$(RAY_CFLAGS)" \
		LINUX_RAY_LDLIBS="$(RAY_LDLIBS)" \
		LINUX_RAY_SDL_LDLIBS="$(RAY_SDL_LDLIBS)" \
		LINUX_RAY_SDL_INCLUDE_DIR="$(RAY_SDL_INCLUDE_DIR)"

linux-aarch64: | $(LINUX_BIN_DIR) $(LINUX_OBJ_DIR)
	@if [ -z "$(AARCH64_CC)" ] || [ -z "$(AARCH64_AR)" ] || [ -z "$(AARCH64_RAY_CFLAGS)" ] || [ -z "$(AARCH64_RAY_LDLIBS)" ] || [ -z "$(AARCH64_RAY_SDL_LDLIBS)" ] || [ -z "$(AARCH64_RAY_SDL_INCLUDE_DIR)" ]; then \
		echo "AARCH64 cross-build variables are missing. Enter the flake shell with 'nix develop'."; \
		exit 1; \
	fi
	$(MAKE) build-linux-arch \
		ARCH_NAME=aarch64 \
		LINUX_CC="$(AARCH64_CC)" \
		LINUX_AR="$(AARCH64_AR)" \
		LINUX_RAY_CFLAGS="$(AARCH64_RAY_CFLAGS)" \
		LINUX_RAY_LDLIBS="$(AARCH64_RAY_LDLIBS)" \
		LINUX_RAY_SDL_LDLIBS="$(AARCH64_RAY_SDL_LDLIBS)" \
		LINUX_RAY_SDL_INCLUDE_DIR="$(AARCH64_RAY_SDL_INCLUDE_DIR)"

build-linux-arch:
	@mkdir -p $(LINUX_OBJ_DIR)/$(ARCH_NAME) $(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib $(LINUX_BIN_DIR)
	$(MAKE) $(FONT_FILES)
	$(MAKE) $(EMBEDDED_ASSETS_C)
	@rm -rf $(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib-src
	@mkdir -p $(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib-src
	cp -R $(RAYLIB_DIR)/. $(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib-src/
	$(MAKE) -j1 -C $(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib-src \
		CC="$(LINUX_CC)" \
		AR="$(LINUX_AR)" \
		PLATFORM=PLATFORM_DESKTOP_SDL \
		GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC \
		RAYLIB_RELEASE_PATH=../raylib \
		RAYLIB_MODULE_AUDIO=$(FLINT_RAYLIB_MODULE_AUDIO) \
		RAYLIB_MODULE_MODELS=FALSE \
		SDL_INCLUDE_PATH="$(LINUX_RAY_SDL_INCLUDE_DIR)" \
		SDL_LIBRARIES="$(LINUX_RAY_SDL_LDLIBS)" \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(LINUX_RAY_CFLAGS) $(APP_RAYLIB_CONFIG) -Os -ffunction-sections -fdata-sections"
	@if [ -n "$(strip $(CORE_SRCS))" ]; then \
		for src in $(CORE_SRCS); do \
			obj="$(LINUX_OBJ_DIR)/$(ARCH_NAME)/core/$${src%.c}.o"; \
			mkdir -p "$$(dirname "$$obj")"; \
			$(LINUX_CC) $(CFLAGS) $(CORE_INCLUDE) -c "$$src" -o "$$obj"; \
		done; \
		find $(LINUX_OBJ_DIR)/$(ARCH_NAME)/core -name '*.o' -print | sort | xargs $(LINUX_AR) rcs $(LINUX_OBJ_DIR)/$(ARCH_NAME)/lib$(APP_NAME)-core.a; \
	fi
	$(LINUX_CC) $(CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(CORE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		$(FLINT_NATIVE_CFLAGS) \
		$(LINUX_RAY_CFLAGS) \
		$(FLINT_NATIVE_SUPPORT_FLAGS) \
		-o $(LINUX_BIN_DIR)/$(APP_NAME)-linux-$(ARCH_NAME) \
		$(SRC) \
		$(FLINT_SRCS) \
		$(if $(strip $(CORE_SRCS)),$(LINUX_OBJ_DIR)/$(ARCH_NAME)/lib$(APP_NAME)-core.a,) \
		$(LINUX_OBJ_DIR)/$(ARCH_NAME)/raylib/libraylib.a \
		$(LINUX_RAY_LDLIBS) \
		$(FLINT_RUNTIME_ASSET_LDLIBS) \
		$(FLINT_NATIVE_LDLIBS) \
		$(FLINT_NATIVE_SYSTEM_LDLIBS) \
		$(LDFLAGS)
