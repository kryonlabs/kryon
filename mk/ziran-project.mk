# Shared native application build route configured by ziran.toml.
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

ifneq ($(filter $(PROJECT_BACKEND)/$(PROJECT_CODEGEN),terminal/c99 desktop/c99 canvas/c99 desktop/zx canvas/zx),$(PROJECT_BACKEND)/$(PROJECT_CODEGEN))
$(error backend $(PROJECT_BACKEND) with codegen $(PROJECT_CODEGEN) is not available)
endif

CC ?= cc
ZIRAN_BUILD_DIR ?= $(abspath $(ZIRAN_DIR)/build)
ZIRAN_MAKE_FLAGS := $(if $(filter $(abspath $(ZIRAN_DIR)/build),$(ZIRAN_BUILD_DIR)),,BUILD_DIR=$(ZIRAN_BUILD_DIR))
# The tool already resolved this profile's pinned or overridden toolchain.
# A parent application's command-line ZIRAN must not replace that absolute
# executable with a bare make prerequisite such as "ziran".
override ZIRAN := $(ZIRAN_BUILD_DIR)/bin/ziran
# Examples share the owning repository's output tree instead of creating a
# build folder beside every example's source.
EXAMPLE_RELATIVE := $(patsubst $(realpath $(KRYON_DIR))/examples/%,%,$(realpath $(CURDIR)))
PROJECT_BUILD_DIR ?= $(if $(filter $(realpath $(KRYON_DIR))/examples/%,$(realpath $(CURDIR))),$(KRYON_DIR)/build/examples/$(EXAMPLE_RELATIVE),build)
GEN_DIR := $(PROJECT_BUILD_DIR)/generated/$(PROJECT_PROFILE)
IR_DIR := $(GEN_DIR)/ir
IR_STAMP := $(IR_DIR)/.complete
C_DIR := $(GEN_DIR)/c
C_STAMP := $(C_DIR)/.complete
PROGRAM := $(PROJECT_BUILD_DIR)/$(PROJECT_NAME)-$(PROJECT_PROFILE)
ifeq ($(PROJECT_CODEGEN),zx)
PROGRAM := $(PROGRAM).zx
ifneq ($(strip $(PROJECT_LIBRARY)$(PROJECT_STATIC_ARCHIVE)$(PROJECT_NATIVE_ARTIFACTS)),)
$(error a Zx profile cannot link a native library or static archive)
endif
endif
BROWSER_BACKEND := $(filter $(PROJECT_BACKEND),canvas)
ifneq ($(BROWSER_BACKEND),)
ifeq ($(PROJECT_CODEGEN),c99)
PROGRAM := $(PROJECT_BUILD_DIR)/$(PROJECT_NAME)-$(PROJECT_PROFILE).html
endif
endif
# The program's name for its window, page and menu entry: the install name or
# else the package name. The kryon tool passes it as KRYON_PROGRAM_TITLE, read
# only as a shell variable, so the text cannot change a recipe. Native and ZX
# programs keep it beside them (src/backend/program_title.zi reads it there);
# pages carry it in their HTML shell.
PROGRAM_TITLE_FILE := $(if $(filter %.html,$(PROGRAM)),,$(if $(filter %.zx,$(PROGRAM)),$(PROGRAM:.zx=.title),$(PROGRAM).title))
PAGE_SHELL := $(if $(BROWSER_BACKEND),$(GEN_DIR)/page-shell.html)
PAGE_SHELL_SOURCE := $(KRYON_DIR)/mk/$(if $(filter zx,$(PROJECT_CODEGEN)),zx,$(PROJECT_BACKEND))-shell.html
PROJECT_CONFIG_FILES := ziran.toml ziran.lock
# The test driver is test code: a build includes it only when
# KRYON_TEST_DRIVER is set in its environment, as it is when the program runs
# under a test. Saved IR folds the choice, so each setting has its own stamp.
TEST_DRIVER_STATE := $(if $(KRYON_TEST_DRIVER),on,off)
# A zx bundle's runtime pictures (kryon/bitmap_image) belong to its player.
ZIRAN_MODULE_ARGS := --project $(if $(KRYON_TEST_DRIVER),--define KRYON_TEST_DRIVER) $(if $(filter zx,$(PROJECT_CODEGEN)),--define KRYON_ZX)
# The CLI quotes each --module-path token in this environment value. Expand it
# only in the shell, keeping make and recursive make from interpreting paths.
ZIRAN_MODULE_PATHS = eval "set -- $$KRYON_MODULE_PATH_ARGS";
ifeq ($(PROJECT_BACKEND),terminal)
ZIRAN_MODULE_ARGS += --define KRYON_TERMINAL
endif
ifeq ($(PROJECT_BACKEND)/$(PROJECT_CODEGEN),desktop/c99)
ifeq ($(shell uname -s),Linux)
# The desktop host needs Linux settings even when the frame IR has no target OS.
ZIRAN_MODULE_ARGS += --define KRYON_LINUX_DESKTOP
endif
endif
ifeq ($(filter $(PROJECT_BACKEND),desktop),)
# A non-desktop C build (terminal, canvas) hosts no desktop
# window; kryon/app then maps the program's window calls to app_unsupported
# so it carries no desktop modules.
ZIRAN_MODULE_ARGS += --define KRYON_NO_DESKTOP_HOST
endif
APP_SOURCES := $(shell find . -type d \( -name build -o -name .git -o -name vendor \) -prune -o -type f \( -name '*.zi' -o -name '*.ts' -o -name '*.tsx' \) -print)
UI_SOURCES := $(shell find $(KRYON_DIR)/src/ui $(KRYON_DIR)/src/tsx $(KRYON_DIR)/examples/shared/screens -type f \( -name '*.zi' -o -name '*.ts' -o -name '*.tsx' \) -print)
HOST_SOURCES := $(wildcard $(KRYON_DIR)/src/backend/$(PROJECT_BACKEND)*.zi $(KRYON_DIR)/src/backend/avif*.zi)
ifneq ($(PROJECT_HOST),)
PROJECT_HOST_PATH := $(realpath $(PROJECT_HOST))
ifeq ($(filter $(CURDIR)/%,$(PROJECT_HOST_PATH)),)
$(error the profile host must stay inside the project directory)
endif
HOST := $(PROJECT_HOST)
HOST_ID := $(basename $(notdir $(PROJECT_HOST)))
HOST_ROOT_SOURCES := $(HOST)
# Ziran's byte-pointer argv has the C argv layout. GCC's hosted-main
# spelling warning does not apply to that generated entry signature.
HOST_CFLAGS := -Wno-main
else
ifneq ($(filter $(PROJECT_BACKEND),desktop),)
HOST_SOURCES += $(KRYON_DIR)/src/backend/app_asset_path.zi $(KRYON_DIR)/src/backend/app_asset.zi $(KRYON_DIR)/src/backend/program_title.zi
HOST_SOURCES += $(KRYON_DIR)/src/backend/system_theme_linux.zi $(KRYON_DIR)/src/backend/parent_theme_linux.zi
endif
ifneq ($(filter $(PROJECT_BACKEND),desktop canvas),)
HOST_SOURCES += $(KRYON_DIR)/src/ui/frame_input_provider.zi
endif
# Entry signatures are checked in common IR, independent of source language.
PROJECT_ENTRY_MODULE := $(basename $(notdir $(PROJECT_ENTRY)))
ifeq ($(PROJECT_ENTRY_MODULE),module)
PROJECT_ENTRY_MODULE := $(notdir $(patsubst %/,%,$(dir $(PROJECT_ENTRY))))
endif
HOST_ID := $(PROJECT_ENTRY_MODULE)
HOST_ROOT_SOURCES := $(PROJECT_ENTRY)
endif
ifeq ($(PROJECT_CODEGEN)/$(PROJECT_BACKEND),zx/canvas)
# Portable app IR is identical on desktop and web. Only the renderer shell
# is compiled for the browser.
ZX_CANVAS_DIR := $(abspath $(GEN_DIR)/player)
ZX_CANVAS_PROGRAM := $(abspath $(PROJECT_BUILD_DIR)/$(PROJECT_NAME)-$(PROJECT_PROFILE).html)
ZX_CANVAS_BUNDLE := $(abspath $(PROGRAM))
ZX_CANVAS_SHELL := $(PAGE_SHELL)
include $(KRYON_DIR)/mk/zx-canvas.mk
BROWSER_PROGRAM := $(ZX_CANVAS_PROGRAM)
endif
# Saved IR folds `#if OS == .WEB` when it is written, so browser IR names its
# target; untargeted IR is portable and selects no web host.
WEB_TARGET := --target=c --target-os web --target-cpu wasm32
ifneq ($(if $(filter c99,$(PROJECT_CODEGEN)),$(BROWSER_BACKEND)),)
ifeq ($(PROJECT_BACKEND),canvas)
HOST_ROOT_SOURCES += $(KRYON_DIR)/src/backend/canvas_raster.zi
endif
# Browser hosts resolve the app's packages like native ones. The checked
# closed link retains the entry and the page's exported runtime callbacks.
IR_COMMAND := $(ZIRAN_MODULE_PATHS) $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) "$$@" $(WEB_TARGET) --define PLATFORM_WEB \
	--entry $(HOST_ID):main -o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN_BUILD_DIR)/bin/zi2c --define PLATFORM_WEB --target-os web --target-cpu wasm32 \
	--emit-main --entry $(HOST_ID):main \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/*.zir

else
# A program selects its kryon/app host by compiled target, so a C program's
# IR names the native C target; a ZX program's and frame IR stay portable.
IR_COMMAND := $(ZIRAN_MODULE_PATHS) $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) "$$@" $(if $(filter c99,$(PROJECT_CODEGEN)),--target=c --define POSIX_THREADS) --entry $(HOST_ID):main -o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN) build --target=c --emit-main --entry $(HOST_ID):main \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/*.zir
endif
ifeq ($(PROJECT_BACKEND),desktop)
HOST_SOURCES += $(KRYON_DIR)/src/backend/cairo_raster.zi $(KRYON_DIR)/src/backend/image_decode_desktop.zi $(KRYON_DIR)/src/backend/avif_decode.zi $(KRYON_DIR)/src/backend/accessibility_linux.zi $(KRYON_DIR)/src/backend/atspi_protocol.zi
HOST_SOURCES += $(wildcard $(KRYON_DIR)/src/backend/gtk_*_linux.zi) $(KRYON_DIR)/src/backend/file_date_linux.zi
endif
ifneq ($(BROWSER_BACKEND),)
WEB_LIBS := --js-library $(ZIRAN_DIR)/web/ziran_web.js -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS
include $(KRYON_DIR)/mk/emcc.mk
# Ordinary Canvas applications may embed portable Ziran bundles through
# std/bundle_host. Build that target's canonical runtime only when needed.
CANVAS_RUNTIME := $(abspath $(GEN_DIR)/runtime/libziran.a)
CANVAS_RUNTIME_SOURCES := $(ZIRAN_DIR)/Makefile $(wildcard $(ZIRAN_DIR)/cmd/*.zi $(ZIRAN_DIR)/cmd/zir*/*.[ch] $(ZIRAN_DIR)/cmd/zi2*/*.[ch] $(ZIRAN_DIR)/cmd/zx2*/*.[ch] $(ZIRAN_DIR)/include/*.h)
CANVAS_RUNTIME_CACHE := $(KRYON_DIR)/build/emscripten-cache/canvas

$(CANVAS_RUNTIME): $(CANVAS_RUNTIME_SOURCES) $(KRYON_DIR)/mk/ziran-project.mk
	EM_CACHE=$(CANVAS_RUNTIME_CACHE) $(MAKE) -B -C $(ZIRAN_DIR) \
		BUILD_DIR=$(abspath $(GEN_DIR)/runtime) CC=$(EMCC) AR=$(dir $(EMCC))emar \
		OBJCOPY=true CFLAGS=-O2 $(CANVAS_RUNTIME)
endif
ZIRAN_STD_SOURCES := $(wildcard $(ZIRAN_DIR)/std/*.zi)
PROJECT_SOURCE_DEPS := $(PROJECT_CONFIG_FILES) $(PROJECT_ENTRY) $(APP_SOURCES) $(PROJECT_EXTRA_SOURCES) $(UI_SOURCES) $(HOST_ROOT_SOURCES) $(HOST_SOURCES) $(ZIRAN_STD_SOURCES)
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
include $(KRYON_DIR)/mk/codecs.mk
HOST_DEPS := $(AVIF_SO)
HOST_LIBS := -Wl,--no-as-needed $(AVIF_SO) -Wl,--as-needed -Wl,-rpath,$(AVIF_BUILD) -Wl,-rpath,'$$ORIGIN/../lib'
HOST_LIBS += $(shell pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig gio-2.0 x11)
HOST_LIBS += $(or $(shell pkg-config --libs libcurl 2>/dev/null),$(if $(filter Linux,$(shell uname -s)),-l:libcurl.so.4,-lcurl))
RUN_ENV := KRYON_FONT_PATH=$(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
endif
ifneq ($(BROWSER_BACKEND),)
EM_CACHE ?= $(KRYON_DIR)/build/emscripten-cache
WEB_PAGE_ASSET_ENTRIES := $(strip $(PROJECT_ASSETS) $(PROJECT_PRELOAD))
WEB_PAGE_ASSET_SOURCES := $(foreach entry,$(WEB_PAGE_ASSET_ENTRIES),$(firstword $(subst @, ,$(entry))))
$(foreach source,$(WEB_PAGE_ASSET_SOURCES),$(if $(filter $(CURDIR)/%,$(realpath $(source))),,$(error preload $(source) must exist inside the project directory)))
WEB_PAGE_ASSET_FILES := $(sort $(if $(WEB_PAGE_ASSET_SOURCES),$(shell find $(WEB_PAGE_ASSET_SOURCES) -type f 2>/dev/null)))
WEB_PAGE_ASSET_ARGS := $(foreach entry,$(WEB_PAGE_ASSET_ENTRIES),\
	$(if $(findstring @,$(entry)),\
		$(firstword $(subst @, ,$(entry)))@/$(lastword $(subst @, ,$(entry))),\
		$(entry)@/$(entry)))
WEB_PAGE_EMBED_ARGS := $(WEB_PAGE_ASSET_ARGS)
HOST_DEPS += $(WEB_PAGE_ASSET_FILES) $(ZIRAN_DIR)/web/ziran_web.js
ifeq ($(PROJECT_BACKEND),canvas)
WEB_PAGE_EMBED_ARGS := $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf $(WEB_PAGE_EMBED_ARGS)
HOST_DEPS += $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
endif
endif

# The package driver validates these system names and preserves manifest order.
# Portable and browser profiles do not consume native system libraries.
ifeq ($(PROJECT_CODEGEN),c99)
ifeq ($(BROWSER_BACKEND),)
HOST_LIBS += $(addprefix -l,$(PROJECT_LINKS))
endif
endif

ifneq ($(PROJECT_LIBRARY),)
HOST_LIBS += -l$(PROJECT_LIBRARY)
endif
ifneq ($(PROJECT_STATIC_ARCHIVE),)
ifneq ($(BROWSER_BACKEND),)
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
toolchain: $(ZIRAN_BUILD_DIR)/bin/ziran $(ZIRAN_BUILD_DIR)/bin/zi2zir $(ZIRAN_BUILD_DIR)/bin/zi2c

$(ZIRAN_BUILD_DIR)/bin/ziran $(ZIRAN_BUILD_DIR)/bin/zi2zir $(ZIRAN_BUILD_DIR)/bin/zi2c:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) all

# Native assets use the same executable-relative layout before and after install.
.PHONY: native-assets
native-assets:
ifeq ($(PROJECT_CODEGEN),c99)
ifeq ($(BROWSER_BACKEND),)
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

$(ZIRAN_BUILD_DIR)/bin/zi2zx $(ZIRAN_BUILD_DIR)/libziran.a:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) all

$(PROGRAM): $(IR_STAMP) $(ASSET_DEPENDENCY) $(ZIRAN_BUILD_DIR)/bin/zi2zx
	$(ZIRAN) bundle --root $(IR_DIR) --entry $(HOST_ID):main \
		$(if $(PROJECT_ASSETS),--asset-dir $(PROJECT_ASSETS)=$(PROJECT_ASSETS)) \
		-o $@ $(IR_DIR)/*.zir
endif

$(GEN_DIR)/test-driver.$(TEST_DRIVER_STATE):
	@mkdir -p $(GEN_DIR)
	@rm -f $(GEN_DIR)/test-driver.on $(GEN_DIR)/test-driver.off
	@: > $@

# Fingerprint external module contents as well as their paths. These files can
# contain spaces and make metacharacters, so keep them out of prerequisite lists.
# Additions, removals and edits with preserved mtimes must invalidate saved IR.
.PHONY: project-toolchain-config
$(GEN_DIR)/toolchain: project-toolchain-config
	@mkdir -p $(GEN_DIR)
	@module_inputs=$$(python3 "$(KRYON_DIR)/scripts/module_path_inputs.py") || exit $$?; \
		printf '%s\n' "$(ZIRAN_BUILD_DIR)" "$$KRYON_MODULE_PATH_ARGS" "$$module_inputs" > $@.new
	@cmp -s $@.new $@ && rm -f $@.new || mv -f $@.new $@

$(IR_STAMP): $(GEN_DIR)/toolchain $(GEN_DIR)/test-driver.$(TEST_DRIVER_STATE) $(PROJECT_SOURCE_DEPS) $(ZIRAN_BUILD_DIR)/bin/zi2zir $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk $(KRYON_DIR)/scripts/module_path_inputs.py
	mkdir -p $(IR_DIR)
	rm -f $(IR_DIR)/*.zir $(IR_STAMP)
	$(IR_COMMAND)
	test -f $(IR_DIR)/$(HOST_ID).zir
	: > $(IR_STAMP)

ifeq ($(PROJECT_CODEGEN),c99)

$(C_STAMP): $(IR_STAMP) $(ZIRAN_BUILD_DIR)/bin/zi2c $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(C_DIR)
	rm -f $(C_DIR)/*.c $(C_DIR)/*.h $(C_STAMP) $(GEN_DIR)/*.c $(GEN_DIR)/*.h $(GEN_DIR)/.complete
	$(C_COMMAND)
	test -f $(C_DIR)/$(HOST_ID).c
	: > $(C_STAMP)

# Editors read how the generated C is compiled from compile_commands.json.
$(GEN_DIR)/compile_commands.json: $(C_STAMP) $(ZIRAN)
	@$(ZIRAN) compile-commands --include $(ZIRAN_DIR)/include \
		--compiler '$(CC) -O2 -ffunction-sections -fdata-sections' \
		-o $@ $(C_DIR) || { echo "kryon: this Kryon needs a newer Ziran; run ziran update ziran" >&2; exit 1; }

# A page carries two builds of the program: one with WebAssembly SIMD128,
# which std/resample's Lanczos kernels use, and a baseline one for browsers
# without it. mk/web-page.sh adds the loader that starts the one the
# browser validates, and carries the font once for both.
# Deep app frames share the portable renderer suspension stack budget.
WEB_LINK = EM_CACHE=$(EM_CACHE) $(EMCC) -O2 -I$(ZIRAN_DIR)/include -iquote $(C_DIR) \
	$(C_DIR)/*.c $(WEB_LIBS) \
	-sASYNCIFY -sASYNCIFY_STACK_SIZE=1048576 -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web -sSTACK_SIZE=8388608 \
	-sALLOW_MEMORY_GROWTH=1

# Normalize output modes on the open descriptor: restricted builds may
# deny path chmod and timestamp syscalls, and a restrictive umask must not
# leave published pages and executables unreadable or unrunnable.
$(PROGRAM): $(C_STAMP) $(GEN_DIR)/compile_commands.json $(HOST_DEPS) $(PAGE_SHELL) $(if $(PAGE_SHELL),$(KRYON_DIR)/mk/web-page.sh) $(CANVAS_RUNTIME_SOURCES) $(KRYON_DIR)/mk/ziran-project.mk
ifneq ($(BROWSER_BACKEND),)
	@runtime=; \
		if test -f "$(C_DIR)/std_bundle_host.c"; then \
			$(MAKE) --no-print-directory -f $(KRYON_DIR)/mk/ziran-project.mk $(CANVAS_RUNTIME) || exit $$?; \
			runtime="$(CANVAS_RUNTIME)"; \
		fi; \
		modules=$$(mktemp -d $(PROJECT_BUILD_DIR)/$(PROJECT_NAME).XXXXXX); \
		trap 'rm -rf "$$modules"' EXIT; \
		$(WEB_LINK) $$runtime -msimd128 -o "$$modules/simd128.js" && \
		$(WEB_LINK) $$runtime -o "$$modules/baseline.js" && \
		sh $(KRYON_DIR)/mk/web-page.sh $(PAGE_SHELL) "$$modules/simd128.js" "$$modules/baseline.js" \
				$(WEB_PAGE_EMBED_ARGS) > "$$modules/page.html" && \
		python3 -c 'import os, sys; descriptor = os.open(sys.argv[1], os.O_RDONLY | os.O_NOFOLLOW); os.fchmod(descriptor, 0o644)' "$$modules/page.html" && \
		mv "$$modules/page.html" $@

else
	@temporary=$$(mktemp $(PROJECT_BUILD_DIR)/$(PROJECT_NAME).XXXXXX.html); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(PROJECT_ARTIFACT_LINK) $(CC) -std=c99 -pedantic-errors $(HOST_CFLAGS) -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(C_DIR) $(C_DIR)/*.c \
		-Wl,--gc-sections $(PROJECT_ARTIFACT_LIBS) $(HOST_LIBS) $(LDLIBS) -lm -o "$$temporary" && \
		python3 -c 'import os, sys; descriptor = os.open(sys.argv[1], os.O_RDONLY | os.O_NOFOLLOW); os.fchmod(descriptor, 0o755)' "$$temporary" && \
		mv "$$temporary" $@
endif
endif

run: build
ifeq ($(PROJECT_CODEGEN),zx)
ifeq ($(PROJECT_BACKEND),desktop)
run: zx-player
	$(RUN_ENV) $(PLAYER) $(PROGRAM)
else ifeq ($(PROJECT_BACKEND),canvas)
	@echo "Kryon ZX browser app: $(BROWSER_PROGRAM)"
else
	$(ZIRAN) run $(PROGRAM)
endif
else
ifneq ($(BROWSER_BACKEND),)
	@echo "Kryon browser app: $(PROGRAM)"
else
.PHONY: run-built
run run-built:
	$(RUN_ENV) ./$(PROGRAM)
endif
endif

# Check the complete bound graph through the same IR route as a build.
check: toolchain
	$(ZIRAN_MODULE_PATHS) $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) "$$@" -o $(GEN_DIR)/check-ir --entry $(HOST_ID):main $(if $(filter c99,$(PROJECT_CODEGEN)),$(if $(BROWSER_BACKEND),$(WEB_TARGET) --define PLATFORM_WEB,--target=c)) $(HOST_ROOT_SOURCES)

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
ifneq ($(BROWSER_BACKEND),)
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
