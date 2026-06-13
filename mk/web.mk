WEB_CC ?= emcc
WEB_AR ?= emar
WEB_WORK_TARGET = $(WEB_BUILD_DIR)/index.html
WEB_TARGET = $(WEB_DIST_DIR)/index.html
WEB_RAYLIB_BUILD_DIR = $(WEB_BUILD_DIR)/raylib
WEB_RAYLIB_A = $(WEB_RAYLIB_BUILD_DIR)/libraylib.web.a
WEB_FLINT_SRCS = $(filter-out $(FLINT_DIR)/src/flint_file_dialog.c,$(FLINT_SRCS))
WEB_RAYLIB_OBJS = \
	$(WEB_RAYLIB_BUILD_DIR)/rcore.o \
	$(WEB_RAYLIB_BUILD_DIR)/rshapes.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtextures.o \
	$(WEB_RAYLIB_BUILD_DIR)/rtext.o \
	$(WEB_RAYLIB_BUILD_DIR)/raudio.o
WEB_CFLAGS = -Wall -Wextra -std=gnu99 -Os -DPLATFORM_WEB -DGRAPHICS_API_OPENGL_ES2 -D_DEFAULT_SOURCE -DSUPPORT_MODULE_RAUDIO=1 -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_OGG=1 -DSUPPORT_FILEFORMAT_MP3=1 -DFLINT_EMBEDDED_ONLY=1
WEB_SHELL = src/web_shell.html
WEB_PUBLIC_FILES = $(wildcard manifest.json) $(shell find web-assets -type f 2>/dev/null)
WEB_LDFLAGS = -sUSE_GLFW=3 -sFETCH=1 -sALLOW_MEMORY_GROWTH=1 -lidbfs.js --shell-file $(WEB_SHELL)

$(WEB_RAYLIB_BUILD_DIR):
	mkdir -p $@

web: $(WEB_TARGET) $(WEB_ZIP)

$(WEB_RAYLIB_BUILD_DIR)/%.o: $(RAYLIB_DIR)/%.c | $(WEB_RAYLIB_BUILD_DIR)
	$(WEB_CC) \
		-c $< \
		-o $@ \
		-Wall \
		-D_GNU_SOURCE \
		-DPLATFORM_WEB \
		-DGRAPHICS_API_OPENGL_ES2 \
		-Wno-missing-braces \
		-Werror=pointer-arith \
		-fno-strict-aliasing \
		-std=gnu99 \
		-D_DEFAULT_SOURCE \
		$(APP_RAYLIB_CONFIG) \
		-Os \
		-ffunction-sections \
		-fdata-sections \
		-I$(RAYLIB_DIR)

$(WEB_RAYLIB_A): $(RAYLIB_SOURCES) $(WEB_RAYLIB_OBJS)
	$(WEB_AR) rcs $@ $(WEB_RAYLIB_OBJS)

$(WEB_WORK_TARGET): $(BUILD_MAKEFILES) $(SRC) $(WEB_FLINT_SRCS) $(CORE_SRCS) $(WEB_SHELL) $(WEB_RAYLIB_A) $(WEB_PUBLIC_FILES) | $(WEB_BUILD_DIR)
	$(WEB_CC) $(WEB_CFLAGS) \
		$(APP_INCLUDE) \
		$(FLINT_INCLUDE) \
		$(CORE_INCLUDE) \
		-I$(RAYLIB_DIR) \
		-o $@ \
		$(SRC) \
		$(WEB_FLINT_SRCS) \
		$(CORE_SRCS) \
		$(WEB_RAYLIB_A) \
		$(WEB_LDFLAGS)
	cache_buster=$$(find $(WEB_BUILD_DIR) -maxdepth 1 \( -name 'index.js' -o -name 'index.data' -o -name 'index.wasm' \) -type f -print | sort | xargs cksum | cksum | cut -d ' ' -f 1); \
		sed -i "s#src=\"index.js\"#src=\"index.js?v=$$cache_buster\"#; s#WEB_CACHE_BUSTER#$$cache_buster#g" $(WEB_WORK_TARGET)
	@if [ -d web-assets ]; then rsync -a web-assets/ $(WEB_BUILD_DIR)/web-assets/; fi
	@if [ -f manifest.json ]; then cp manifest.json $(WEB_BUILD_DIR)/; fi

$(WEB_TARGET): $(WEB_WORK_TARGET) | $(WEB_DIST_DIR)
	@if [ "$(WEB_BUILD_DIR)" != "$(BUILD_DIR)/web" ]; then rm -rf $(BUILD_DIR)/web; fi
	rm -rf $(WEB_DIST_DIR)
	mkdir -p $(WEB_DIST_DIR)
	find $(WEB_BUILD_DIR) -maxdepth 1 -type f \
		\( -name '*.html' -o -name '*.js' -o -name '*.wasm' -o -name '*.data' -o -name '*.json' -o -name '*.png' -o -name '*.ico' -o -name '*.webmanifest' \) \
		-exec cp {} $(WEB_DIST_DIR)/ \;
	@if [ -d $(WEB_BUILD_DIR)/web-assets ]; then cp -R $(WEB_BUILD_DIR)/web-assets $(WEB_DIST_DIR)/; fi

$(WEB_ZIP): $(WEB_TARGET)
	rm -f $@
	cd $(WEB_DIST_DIR) && zip -r $(abspath $@) .
