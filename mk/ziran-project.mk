# Shared build route for a Ziran application configured by ziran.toml.
ifndef PROJECT_NAME
$(error PROJECT_NAME is required; run this through ziran tool kryon)
endif
ifndef PROJECT_ENTRY
$(error PROJECT_ENTRY is required; run this through ziran tool kryon)
endif
ifndef KRYON_DIR
$(error KRYON_DIR is required; run this through ziran tool kryon)
endif
ifndef ZIRAN_DIR
$(error ZIRAN_DIR is required; run this through ziran tool kryon)
endif
ifndef PROJECT_PROFILE
$(error PROJECT_PROFILE is required; run this through ziran tool kryon)
endif
ifndef ZIRAN_PACKAGE_ID
$(error ZIRAN_PACKAGE_ID is required; run this through ziran tool kryon)
endif

ifneq ($(filter $(PROJECT_BACKEND)/$(PROJECT_CODEGEN),terminal/c99 desktop/c99 raylib/c99 canvas/c99 dom/c99 pixmap/c99 desktop/zx pixmap/zx canvas/zx raylib/zx),$(PROJECT_BACKEND)/$(PROJECT_CODEGEN))
$(error backend $(PROJECT_BACKEND) with codegen $(PROJECT_CODEGEN) is not available)
endif

CC ?= cc
# The tool already resolved this profile's pinned or overridden toolchain.
# A parent application's command-line ZIRAN must not replace that absolute
# executable with a bare make prerequisite such as "ziran".
override ZIRAN := $(ZIRAN_DIR)/build/bin/ziran
GEN_DIR := build/generated/$(PROJECT_PROFILE)
IR_DIR := $(GEN_DIR)/ir
IR_STAMP := $(IR_DIR)/.complete
C_DIR := $(GEN_DIR)/c
C_STAMP := $(C_DIR)/.complete
PROGRAM := build/$(PROJECT_NAME)-$(PROJECT_PROFILE)
ifeq ($(PROJECT_CODEGEN),zx)
PROGRAM := $(PROGRAM).zx
ifneq ($(strip $(PROJECT_LIBRARY)$(PROJECT_STATIC_ARCHIVE)$(PROJECT_NATIVE_ARTIFACTS)),)
$(error a Zx profile cannot link a native library or static archive)
endif
endif
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
ifeq ($(PROJECT_CODEGEN),c99)
PROGRAM := build/$(PROJECT_NAME)-$(PROJECT_PROFILE).html
endif
endif
# platform = "web" builds the raylib host as a WebGL browser page: raylib's
# PLATFORM_WEB library (GLES2 on WebGL 1), linked by Emscripten with ASYNCIFY
# so a host's own `while PollRaylib()` loop yields to the page every frame.
# The page lands as index.html, index.js, index.wasm and, with preloaded
# files, index.data in build/NAME-PROFILE/.
RAYLIB_WEB :=
ifneq ($(PROJECT_PLATFORM),)
ifneq ($(PROJECT_BACKEND)/$(PROJECT_CODEGEN)/$(PROJECT_PLATFORM),raylib/c99/web)
$(error platform $(PROJECT_PLATFORM) needs a raylib profile with c99 codegen)
endif
RAYLIB_WEB := web
WEB_PAGE_DIR := build/$(PROJECT_NAME)-$(PROJECT_PROFILE)
PROGRAM := $(WEB_PAGE_DIR)/index.html
endif
ifneq ($(PROJECT_PRELOAD),)
ifeq ($(RAYLIB_WEB),)
$(error preload needs platform web)
endif
endif
HOST_MODULE := $(PROJECT_BACKEND)_run
ifneq ($(filter $(PROJECT_BACKEND)/$(PROJECT_CODEGEN),desktop/zx canvas/zx raylib/zx),)
HOST_MODULE := zx_run
endif
BROWSER_BACKEND := $(filter $(PROJECT_BACKEND),canvas dom)
# The program's name for its window, page and menu entry: the install name or
# else the package name. The kryon tool passes it as KRYON_PROGRAM_TITLE, read
# only as a shell variable, so the text cannot change a recipe. Native and ZX
# programs keep it beside them (src/backend/program_title.zi reads it there);
# pages carry it in their HTML shell.
PROGRAM_TITLE_FILE := $(if $(filter %.html,$(PROGRAM)),,$(if $(filter %.zx,$(PROGRAM)),$(PROGRAM:.zx=.title),$(PROGRAM).title))
PAGE_SHELL := $(if $(BROWSER_BACKEND),$(GEN_DIR)/page-shell.html)
PAGE_SHELL_SOURCE := $(KRYON_DIR)/mk/$(if $(filter zx,$(PROJECT_CODEGEN)),zx,canvas)-shell.html
ifneq ($(RAYLIB_WEB),)
# The page reads the program's name from its virtual file system.
PROGRAM_TITLE_FILE := $(GEN_DIR)/kryon.title
PAGE_SHELL := $(GEN_DIR)/page-shell.html
PAGE_SHELL_SOURCE := $(KRYON_DIR)/mk/raylib-shell.html
endif
HOST_ID := $(ZIRAN_PACKAGE_ID)_$(HOST_MODULE)
PROJECT_CONFIG_FILES := ziran.toml ziran.lock
ZIRAN_MODULE_ARGS := --project
HOST := $(KRYON_DIR)/src/backend/$(HOST_MODULE).zi
ifneq ($(PROJECT_HOST),)
PROJECT_HOST_PATH := $(realpath $(PROJECT_HOST))
ifeq ($(filter $(CURDIR)/%,$(PROJECT_HOST_PATH)),)
$(error the profile host must stay inside the project directory)
endif
HOST := $(PROJECT_HOST)
HOST_ID := $(basename $(notdir $(PROJECT_HOST)))
# Ziran's byte-pointer argv has the C argv layout. GCC's hosted-main
# spelling warning does not apply to that generated entry signature.
HOST_CFLAGS := -Wno-main
endif
APP_SOURCES := $(wildcard src/*.zi src/*/*.zi)
UI_SOURCES := $(wildcard $(KRYON_DIR)/src/ui/*.zi)
HOST_SOURCES := $(wildcard $(KRYON_DIR)/src/backend/$(PROJECT_BACKEND)*.zi $(KRYON_DIR)/src/backend/avif*.zi)
ifneq ($(filter $(PROJECT_BACKEND),desktop raylib),)
HOST_SOURCES += $(KRYON_DIR)/src/backend/app_asset_path.zi $(KRYON_DIR)/src/backend/app_asset.zi $(KRYON_DIR)/src/backend/program_title.zi
endif
ifneq ($(filter $(PROJECT_BACKEND),desktop canvas dom),)
HOST_SOURCES += $(KRYON_DIR)/src/backend/frame_input_provider.zi
endif
HOST_ROOT_SOURCES := $(HOST)
ifeq ($(PROJECT_CODEGEN)/$(PROJECT_BACKEND),zx/canvas)
# Portable app IR is identical on desktop and web. Only the renderer shell
# is compiled for the browser.
ZX_CANVAS_DIR := $(abspath $(GEN_DIR)/player)
ZX_CANVAS_PROGRAM := $(abspath build/$(PROJECT_NAME)-$(PROJECT_PROFILE).html)
ZX_CANVAS_BUNDLE := $(abspath $(PROGRAM))
ZX_CANVAS_SHELL := $(PAGE_SHELL)
include $(KRYON_DIR)/mk/zx-canvas.mk
BROWSER_PROGRAM := $(ZX_CANVAS_PROGRAM)
endif
ifneq ($(if $(filter c99,$(PROJECT_CODEGEN)),$(BROWSER_BACKEND)),)
HOST_ROOT_SOURCES += $(KRYON_DIR)/src/backend/canvas_raster.zi
# Browser hosts resolve the app's packages like native ones; every saved
# module is lowered because the browser runtime calls into the page.
IR_COMMAND := $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) --define PLATFORM_WEB \
	-o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN_DIR)/build/bin/zi2c --define PLATFORM_WEB \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/*.zir
else ifneq ($(RAYLIB_WEB),)
# The page has no POSIX threads: PLATFORM_WEB selects the single-threaded
# image, Lanczos and file paths the browser runtime provides.
IR_COMMAND := $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) --define PLATFORM_WEB \
	--entry $(HOST_ID):main -o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN_DIR)/build/bin/zi2c --define PLATFORM_WEB --entry $(HOST_ID):main \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/$(HOST_ID).zir
else
IR_COMMAND := $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) $(if $(filter c99,$(PROJECT_CODEGEN)),--define POSIX_THREADS) --entry $(HOST_ID):main -o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN) build --target=c --entry $(HOST_ID):main \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/$(HOST_ID).zir
endif
ifeq ($(PROJECT_BACKEND),desktop)
HOST_SOURCES += $(KRYON_DIR)/src/backend/cairo_raster.zi $(KRYON_DIR)/src/backend/image_decode_linux.zi $(KRYON_DIR)/src/backend/avif_decode.zi $(KRYON_DIR)/src/backend/accessibility_linux.zi $(KRYON_DIR)/src/backend/atspi_protocol.zi
HOST_SOURCES += $(wildcard $(KRYON_DIR)/src/backend/gtk_*_linux.zi) $(KRYON_DIR)/src/backend/file_date_linux.zi
endif
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
CANVAS_LIBS := --js-library $(ZIRAN_DIR)/web/ziran_web.js -sEXPORTED_RUNTIME_METHODS=FS
include $(KRYON_DIR)/mk/emcc.mk
endif
ZIRAN_STD_SOURCES := $(wildcard $(ZIRAN_DIR)/std/*.zi)
PROJECT_SOURCE_DEPS := $(PROJECT_CONFIG_FILES) $(PROJECT_ENTRY) $(APP_SOURCES) $(PROJECT_EXTRA_SOURCES) $(UI_SOURCES) $(HOST) $(HOST_SOURCES) $(ZIRAN_STD_SOURCES)
ifneq ($(PROJECT_ASSETS),)
ASSET_PATH := $(realpath $(PROJECT_ASSETS))
ifeq ($(filter $(CURDIR)/%,$(ASSET_PATH)),)
$(error assets must stay inside the project directory)
endif
.PHONY: asset-content
ASSET_DEPENDENCY := asset-content
endif
HOST_DEPS :=
ifeq ($(PROJECT_BACKEND),desktop)
include $(KRYON_DIR)/mk/raylib-library.mk
HOST_DEPS := $(AVIF_SO)
HOST_LIBS := -Wl,--no-as-needed $(AVIF_SO) -Wl,--as-needed -Wl,-rpath,$(AVIF_BUILD) -Wl,-rpath,'$$ORIGIN/../lib'
HOST_LIBS += $(shell pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig gio-2.0)
HOST_LIBS += $(RAYLIB_HTTP_LIBS)
RUN_ENV := KRYON_FONT_PATH=$(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
endif
ifeq ($(PROJECT_BACKEND),raylib)
include $(KRYON_DIR)/mk/raylib-library.mk
HOST_DEPS := $(RAYLIB_ARCHIVES)
HOST_LIBS := $(RAYLIB_ARCHIVES) $(RAYLIB_PLATFORM_LIBS) $(RAYLIB_AUDIO_LIBS)
ifeq ($(RAYLIB_WEB),)
HOST_LIBS += $(RAYLIB_HTTP_LIBS)
endif
RUN_ENV := KRYON_FONT_PATH=$(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf

endif
ifneq ($(RAYLIB_WEB),)
EM_CACHE ?= $(KRYON_DIR)/build/emscripten-cache
# assets and every preload entry mount at their project-relative path, or at
# MOUNT for DIR@MOUNT, under the page's root, its working directory. Kryon's
# font and the program title sit beside them.
WEB_PRELOAD_ENTRIES := $(strip $(PROJECT_ASSETS) $(PROJECT_PRELOAD))
WEB_PRELOAD_SOURCES := $(foreach entry,$(WEB_PRELOAD_ENTRIES),$(firstword $(subst @, ,$(entry))))
$(foreach source,$(WEB_PRELOAD_SOURCES),$(if $(filter $(CURDIR)/%,$(realpath $(source))),,$(error preload $(source) must exist inside the project directory)))
WEB_PRELOAD_FILES := $(if $(WEB_PRELOAD_SOURCES),$(shell find $(WEB_PRELOAD_SOURCES) -type f 2>/dev/null))
WEB_PRELOAD_ARGS := $(foreach entry,$(WEB_PRELOAD_ENTRIES),--preload-file $(if $(findstring @,$(entry)),$(firstword $(subst @, ,$(entry)))@/$(lastword $(subst @, ,$(entry))),$(entry)@/$(entry))) \
	--preload-file $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf \
	--preload-file $(PROGRAM_TITLE_FILE)@/kryon.title
HOST_DEPS += $(WEB_PRELOAD_FILES) $(PROGRAM_TITLE_FILE) $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
# ziran_web.js carries the browser side of Ziran's page capabilities, such as
# HTTP transfers, for the modules a host imports.
RAYLIB_WEB_LINK = EM_CACHE=$(EM_CACHE) $(EMCC) -O2 -I$(ZIRAN_DIR)/include -iquote $(C_DIR) \
	$(C_DIR)/*.c $(RAYLIB_A) --js-library $(ZIRAN_DIR)/web/ziran_web.js \
	-sUSE_GLFW=3 -sMIN_WEBGL_VERSION=1 -sGL_ENABLE_GET_PROC_ADDRESS=1 \
	-sASYNCIFY -sEXIT_RUNTIME=1 -sENVIRONMENT=web -sSTACK_SIZE=8388608 \
	-sALLOW_MEMORY_GROWTH=1 -sFORCE_FILESYSTEM=1 -sEXPORTED_RUNTIME_METHODS=FS \
	--shell-file $(PAGE_SHELL) $(WEB_PRELOAD_ARGS)
endif
ifeq ($(PROJECT_BACKEND)/$(PROJECT_CODEGEN),raylib/zx)
ZX_RAYLIB_DIR := $(abspath $(GEN_DIR)/player)
ZX_RAYLIB_PROGRAM := $(ZX_RAYLIB_DIR)/zx
ZX_RAYLIB_ARCHIVE := $(RAYLIB_ARCHIVES)
include $(KRYON_DIR)/mk/zx-raylib.mk
NATIVE_PLAYER := $(ZX_RAYLIB_PROGRAM)
endif
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
EM_CACHE ?= $(KRYON_DIR)/build/emscripten-cache
endif

# The package driver validates these system names and preserves manifest order.
# Portable and browser profiles do not consume native system libraries.
ifeq ($(PROJECT_CODEGEN),c99)
ifeq ($(BROWSER_BACKEND)$(RAYLIB_WEB),)
HOST_LIBS += $(addprefix -l,$(PROJECT_LINKS))
endif
endif

ifneq ($(PROJECT_LIBRARY),)
HOST_LIBS += -l$(PROJECT_LIBRARY)
endif
ifneq ($(PROJECT_STATIC_ARCHIVE),)
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
$(error static_archive requires a native profile)
endif
PROJECT_STATIC_ARCHIVE_PATH := $(shell realpath "$(PROJECT_STATIC_ARCHIVE)" 2>/dev/null)
ifeq ($(filter $(CURDIR)/%,$(PROJECT_STATIC_ARCHIVE_PATH)),)
$(error the profile static archive must stay inside the project directory)
endif
ifeq ($(shell test -f "$(PROJECT_STATIC_ARCHIVE_PATH)" && echo yes),)
$(error the profile static archive must be a regular file)
endif
HOST_DEPS += $(PROJECT_STATIC_ARCHIVE)
HOST_LIBS += $(PROJECT_STATIC_ARCHIVE_PATH)
endif

ifeq ($(PROJECT_CODEGEN),c99)
ifeq ($(BROWSER_BACKEND),)
HOST_CFLAGS += -pthread
HOST_LIBS += -pthread
endif
endif

ifneq ($(strip $(PROJECT_NATIVE_ARTIFACTS)),)
ifneq ($(PROJECT_CODEGEN),c99)
$(error native artifacts require a native c99 profile)
endif
ifneq ($(BROWSER_BACKEND),)
$(error native artifacts require a native c99 profile)
endif
ifeq ($(strip $(PROJECT_NATIVE_PLATFORM)),)
$(error native artifacts require an explicit native_platform)
endif
.PHONY: native-artifact-content
HOST_DEPS += native-artifact-content
PROJECT_ARTIFACT_LINK = python3 "$(KRYON_DIR)/scripts/link_artifacts.py" \
	--toolchain "$(ZIRAN_DIR)" --project "$(CURDIR)" \
	--platform "$(PROJECT_NATIVE_PLATFORM)" --artifacts "$(PROJECT_NATIVE_ARTIFACTS)" --
PROJECT_ARTIFACT_LIBS := --kryon-artifact-libraries
endif

.PHONY: run build check install toolchain program-title-config

# Considered on every build, rewritten only when the name changes.
ifneq ($(PROGRAM_TITLE_FILE),)
$(PROGRAM_TITLE_FILE): program-title-config
	@mkdir -p $(dir $@)
	@printf '%s\n' "$${KRYON_PROGRAM_TITLE:-$(PROJECT_NAME)}" > $@.new
	@cmp -s $@.new $@ && rm -f $@.new || mv -f $@.new $@
endif
ifneq ($(PAGE_SHELL),)
$(PAGE_SHELL): $(PAGE_SHELL_SOURCE) $(KRYON_DIR)/mk/page-title.awk program-title-config
	@mkdir -p $(dir $@)
	@KRYON_PAGE_TITLE="$${KRYON_PROGRAM_TITLE:-$(PROJECT_NAME)}" \
		awk -f $(KRYON_DIR)/mk/page-title.awk $(PAGE_SHELL_SOURCE) > $@.new
	@cmp -s $@.new $@ && rm -f $@.new || mv -f $@.new $@
endif


# Project builds run the driver, which shells out to zi2zir and zi2c.
# The pinned toolchain is built once; see the kryon tool's rule.
toolchain: $(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c

$(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) \
		build/bin/ziran build/bin/zi2zir build/bin/zi2c

# Native assets use the same executable-relative layout before and after install.
.PHONY: native-assets
native-assets:
ifeq ($(PROJECT_CODEGEN),c99)
ifeq ($(BROWSER_BACKEND)$(RAYLIB_WEB),)
	sh $(KRYON_DIR)/scripts/copy-native-assets.sh "$(ASSET_PATH)" "$(PROGRAM).assets" "$(PROJECT_ASSETS)"
endif
endif

build: toolchain
	$(MAKE) --no-print-directory -f $(KRYON_DIR)/mk/ziran-project.mk $(PROGRAM) $(PROGRAM_TITLE_FILE) $(BROWSER_PROGRAM) $(NATIVE_PLAYER) \
		PROJECT_NAME=$(PROJECT_NAME) PROJECT_ENTRY=$(PROJECT_ENTRY) \
		PROJECT_BACKEND=$(PROJECT_BACKEND) PROJECT_CODEGEN=$(PROJECT_CODEGEN) \
		PROJECT_PROFILE=$(PROJECT_PROFILE) KRYON_DIR=$(KRYON_DIR) ZIRAN_DIR=$(ZIRAN_DIR)
	$(MAKE) --no-print-directory -f $(KRYON_DIR)/mk/ziran-project.mk native-assets

ifeq ($(PROJECT_CODEGEN),zx)
include $(KRYON_DIR)/mk/zx-player.mk

$(ZIRAN_DIR)/build/bin/zi2zx $(ZIRAN_DIR)/build/libziran.a:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

$(PROGRAM): $(IR_STAMP) $(ASSET_DEPENDENCY) $(ZIRAN_DIR)/build/bin/zi2zx
	$(ZIRAN) bundle --root $(IR_DIR) --entry $(HOST_ID):main \
		$(if $(filter pixmap,$(PROJECT_BACKEND)),--bind-host $(ZIRAN_PACKAGE_ID)_pixmap) \
		$(if $(PROJECT_ASSETS),--asset-dir $(PROJECT_ASSETS)=$(PROJECT_ASSETS)) \
		-o $@ $(IR_DIR)/$(HOST_ID).zir
endif

$(IR_STAMP): $(PROJECT_SOURCE_DEPS) $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(IR_DIR)
	rm -f $(IR_DIR)/*.zir $(IR_STAMP)
	$(IR_COMMAND)
	test -f $(IR_DIR)/$(HOST_ID).zir
	touch $(IR_STAMP)

ifeq ($(PROJECT_CODEGEN),c99)

$(C_STAMP): $(IR_STAMP) $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(C_DIR)
	rm -f $(C_DIR)/*.c $(C_DIR)/*.h $(C_STAMP) $(GEN_DIR)/*.c $(GEN_DIR)/*.h $(GEN_DIR)/.complete
	$(C_COMMAND)
	test -f $(C_DIR)/$(HOST_ID).c
	touch $(C_STAMP)

# Editors read how the generated C is compiled from compile_commands.json.
$(GEN_DIR)/compile_commands.json: $(C_STAMP) $(ZIRAN)
	@$(ZIRAN) compile-commands --include $(ZIRAN_DIR)/include \
		--compiler '$(CC) -O2 -ffunction-sections -fdata-sections' \
		-o $@ $(C_DIR) || { echo "kryon: this Kryon needs a newer Ziran; run ziran update ziran" >&2; exit 1; }

# A page carries two builds of the program: one with WebAssembly SIMD128,
# which std/resample's Lanczos kernels use, and a baseline one for browsers
# without it. mk/web-page.sh adds the loader that starts the one the
# browser validates, and carries the font once for both.
WEB_LINK = EM_CACHE=$(EM_CACHE) $(EMCC) -O2 -I$(ZIRAN_DIR)/include -iquote $(C_DIR) \
	$(C_DIR)/*.c $(CANVAS_LIBS) \
	-sASYNCIFY -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web -sSTACK_SIZE=8388608 \
	-sALLOW_MEMORY_GROWTH=1

$(PROGRAM): $(C_STAMP) $(GEN_DIR)/compile_commands.json $(HOST_DEPS) $(PAGE_SHELL) $(if $(PAGE_SHELL),$(KRYON_DIR)/mk/web-page.sh)
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
	@modules=$$(mktemp -d build/$(PROJECT_NAME).XXXXXX); \
		trap 'rm -rf "$$modules"' EXIT; \
		$(WEB_LINK) -msimd128 -o "$$modules/simd128.js" && \
		$(WEB_LINK) -o "$$modules/baseline.js" && \
		sh $(KRYON_DIR)/mk/web-page.sh $(PAGE_SHELL) "$$modules/simd128.js" "$$modules/baseline.js" \
			$(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf > "$$modules/page.html" && \
		chmod 644 "$$modules/page.html" && mv "$$modules/page.html" $@
else ifneq ($(RAYLIB_WEB),)
	@mkdir -p build
	@page=$$(mktemp -d build/$(PROJECT_NAME).XXXXXX); \
		trap 'rm -rf "$$page" "$$page.old"' EXIT; \
		$(RAYLIB_WEB_LINK) -o "$$page/index.html" && \
		chmod 755 "$$page" && chmod 644 "$$page"/index.* && \
		{ test ! -d $(WEB_PAGE_DIR) || mv $(WEB_PAGE_DIR) "$$page.old"; } && \
		mv "$$page" $(WEB_PAGE_DIR)
else
	@temporary=$$(mktemp build/$(PROJECT_NAME).XXXXXX.html); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(PROJECT_ARTIFACT_LINK) $(CC) -std=c99 -pedantic-errors $(HOST_CFLAGS) -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(C_DIR) $(C_DIR)/*.c \
		-Wl,--gc-sections $(PROJECT_ARTIFACT_LIBS) $(HOST_LIBS) $(LDLIBS) -lm -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@
endif
endif

run: build
ifeq ($(PROJECT_CODEGEN),zx)
ifeq ($(PROJECT_BACKEND),desktop)
run: zx-player
	$(RUN_ENV) $(PLAYER) $(PROGRAM)
else ifeq ($(PROJECT_BACKEND),raylib)
run: zx-raylib
	$(RUN_ENV) $(ZX_RAYLIB_PROGRAM) $(PROGRAM)
else ifeq ($(PROJECT_BACKEND),canvas)
	@echo "Kryon ZX browser app: $(BROWSER_PROGRAM)"
else
	$(ZIRAN) run $(PROGRAM)
endif
else
ifneq ($(RAYLIB_WEB),)
	@echo "Kryon raylib browser app: serve ./$(WEB_PAGE_DIR) over HTTP and open index.html"
else ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
	@echo "Kryon browser app: ./$@"
else
	$(RUN_ENV) ./$(PROGRAM)
endif
endif

# Browser hosts are checked as they are built, for the web platform.
check: toolchain
	$(ZIRAN) check $(ZIRAN_MODULE_ARGS) $(if $(filter c99,$(PROJECT_CODEGEN)),$(if $(BROWSER_BACKEND)$(RAYLIB_WEB),--define PLATFORM_WEB)) $(HOST)

# `ziran install` copies the built program to PREFIX/bin and describes it to
# the desktop. The KRYON_INSTALL_* values come from [tool.kryon.install] and
# are only ever read as quoted shell variables. The program is replaced by
# rename, so a running copy keeps its file and later builds never touch it.
TERMINAL_APP := false
ifeq ($(PROJECT_BACKEND),terminal)
TERMINAL_APP := true
endif

install: build
ifeq ($(PROJECT_CODEGEN),zx)
install: zx-player
endif
ifneq ($(BROWSER_BACKEND)$(RAYLIB_WEB),)
	@echo "kryon: browser profiles cannot be installed" >&2; exit 2
else
	@test -n "$$ZIRAN_INSTALL_PREFIX" && test -n "$$ZIRAN_INSTALL_BIN" || \
		{ echo "kryon: run this through ziran install" >&2; exit 2; }
	@set -e; \
	prefix="$$ZIRAN_INSTALL_PREFIX"; bin="$$ZIRAN_INSTALL_BIN"; \
	program="$$prefix/bin/$$bin"; \
	sh $(KRYON_DIR)/scripts/make-install-dirs.sh "$$prefix/bin" "$$prefix/share/applications"; \
	if [ "$(PROJECT_CODEGEN)" = zx ]; then \
		app="$$prefix/share/kryon/$$bin"; sh $(KRYON_DIR)/scripts/make-install-dirs.sh "$$app"; \
		cp $(PROGRAM) "$$app/app.zx.new"; \
		chmod 644 "$$app/app.zx.new"; \
		mv -f "$$app/app.zx.new" "$$app/app.zx"; \
		cp $(PLAYER) "$$program.new"; \
	else \
		cp $(PROGRAM) "$$program.new"; \
	fi; chmod 755 "$$program.new"; \
	mv -f "$$program.new" "$$program"; echo "$$program"; \
	share="$$prefix/share/kryon/$$bin"; sh $(KRYON_DIR)/scripts/make-install-dirs.sh "$$share"; \
	if [ "$(PROJECT_CODEGEN)" = c99 ]; then \
		sh $(KRYON_DIR)/scripts/copy-native-assets.sh "$(ASSET_PATH)" "$$share/assets" "$(PROJECT_ASSETS)"; \
	fi; \
	cp $(PROGRAM_TITLE_FILE) "$$share/app.title.new"; \
	chmod 644 "$$share/app.title.new"; \
	mv -f "$$share/app.title.new" "$$share/app.title"; echo "$$share/app.title"; \
	icon="$$KRYON_INSTALL_ICON_NAME"; \
	if [ -n "$$KRYON_INSTALL_ICON" ]; then \
		sh $(KRYON_DIR)/scripts/make-install-dirs.sh "$$prefix/share/pixmaps"; \
		icon="$$prefix/share/pixmaps/$$bin.$${KRYON_INSTALL_ICON##*.}"; \
		cp "$(CURDIR)/$$KRYON_INSTALL_ICON" "$$icon.new"; \
		chmod 644 "$$icon.new"; \
		mv -f "$$icon.new" "$$icon"; echo "$$icon"; \
	fi; \
	directory=""; \
	case "$$KRYON_INSTALL_DIRECTORY" in \
		"") ;; .) directory="$(CURDIR)" ;; /*) directory="$$KRYON_INSTALL_DIRECTORY" ;; \
		*) directory="$(CURDIR)/$$KRYON_INSTALL_DIRECTORY" ;; esac; \
	entry() { \
		echo "[Desktop Entry]"; echo "Type=Application"; \
		echo "Name=$${KRYON_INSTALL_NAME:-$(PROJECT_NAME)}"; \
		[ -z "$$KRYON_INSTALL_COMMENT" ] || echo "Comment=$$KRYON_INSTALL_COMMENT"; \
		echo "Exec=$$1"; \
		[ -z "$$directory" ] || echo "Path=$$directory"; \
		[ -z "$$icon" ] || echo "Icon=$$icon"; \
		echo "Terminal=$(TERMINAL_APP)"; \
		[ -z "$$KRYON_INSTALL_CATEGORIES" ] || echo "Categories=$$KRYON_INSTALL_CATEGORIES"; \
		echo "X-Kryon-Installed=true"; \
	}; \
	desktop="$$prefix/share/applications/$$bin.desktop"; \
	entry "\"$$program\"" > "$$desktop.new"; chmod 644 "$$desktop.new"; mv -f "$$desktop.new" "$$desktop"; echo "$$desktop"; \
	autostart="$${XDG_CONFIG_HOME:-$$HOME/.config}/autostart/$$bin.desktop"; \
	if [ "$$KRYON_INSTALL_AUTOSTART" = true ]; then \
		mkdir -p "$${autostart%/*}"; \
		launch="\"$$program\""; \
		[ -z "$$KRYON_INSTALL_AUTOSTART_ENV" ] || launch="env $$KRYON_INSTALL_AUTOSTART_ENV \"$$program\""; \
		{ entry "$$launch"; echo "X-GNOME-Autostart-enabled=true"; } > "$$autostart.new"; \
		mv -f "$$autostart.new" "$$autostart"; echo "$$autostart"; \
	elif [ -f "$$autostart" ] && grep -qx "X-Kryon-Installed=true" "$$autostart"; then \
		rm -f "$$autostart"; echo "removed $$autostart"; \
	fi
endif
