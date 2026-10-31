.DEFAULT_GOAL := all

.PHONY: site site-gallery site-gallery-test
site:
	python3 scripts/build-site.py

site-gallery:
	python3 scripts/build-gallery.py

site-gallery-test:
	python3 scripts/gallery.py
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 node tests/gallery_browser.mjs

CC ?= cc
AR ?= ar
# Project tools execute on the build host, even when artifact CC targets an ABI.
PROJECT_CC ?= cc
BUILD_DIR ?= build/ziran
# Resolve the configured package; CI supplies its already checked-out source.
ZIRAN_DIR ?= $(if $(ZIRAN_ROOT),$(ZIRAN_ROOT),$(shell ziran pkg path ziran))
export ZIRAN_ROOT := $(abspath $(ZIRAN_DIR))
ZIRAN_BUILD_DIR ?= $(abspath $(ZIRAN_DIR)/build)
ZI2ZIR_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2zir
ZI2C_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2c
ZI2GO_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2go
ZIRAN_BIN ?= $(ZIRAN_BUILD_DIR)/bin/ziran
# Ziran's make rebuilds every object when its build directory is spelled
# another way, so its own directory is passed the way it spells it.
ifeq ($(ZIRAN_BUILD_DIR),$(abspath $(ZIRAN_DIR)/build))
ZIRAN_MAKE_FLAGS :=
ZIRAN_TOOL_TARGETS := build/bin/zi2zir build/bin/zi2c build/bin/zi2go
else
ZIRAN_MAKE_FLAGS := BUILD_DIR=$(ZIRAN_BUILD_DIR)
ZIRAN_TOOL_TARGETS := $(ZI2ZIR_BIN) $(ZI2C_BIN) $(ZI2GO_BIN)
endif
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
ZIRAN_STD_PATH := --module-path $(ZIRAN_DIR)/std
KRYON_DIR := $(CURDIR)
include mk/zx-player.mk
ZIRAN_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.c \
    $(ZIRAN_DIR)/cmd/zir*/*.h $(ZIRAN_DIR)/include/*.h \
    $(ZIRAN_DIR)/scripts/* $(ZIRAN_DIR)/std/*.zi $(ZIRAN_DIR)/Makefile)

SOURCE := $(addprefix src/ui/,$(shell cat src/ui/modules.txt))
MODULES := $(basename $(notdir $(SOURCE))) utf8 grapheme grapheme_data
OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(addsuffix .o,$(MODULES)))

.PHONY: all backends-check check laws test test-focus ziran-test header-check source-check docs-check public-surface-check clean project-toolchain project-test templates-test pixmap-parity-test pixmap-font pixmap-emoji install-user dom-project-test host-keys-test
all: source-check $(BUILD_DIR)/libkryon.a backends-check

# Project command. Its implementation and platform integration are Ziran.
project-toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) all

# The kryon tool builds its Ziran toolchain only when it is missing, as ziran
# builds a pinned one: a locked toolchain never changes, and a local one is
# rebuilt in its own checkout.
$(ZIRAN_DIR)/build/bin/zi2c:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

build/project/gen/cli_linux.c: src/project/cli_linux.zi src/project/options.zi $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN_DIR)/build/bin/ziran
	mkdir -p build/project/gen
	rm -f build/project/gen/*.c build/project/gen/*.h
	$(ZIRAN_DIR)/build/bin/ziran build --target=c --root src/project \
		-o build/project/gen src/project/cli_linux.zi

build/bin/kryon: build/project/gen/cli_linux.c Makefile
	mkdir -p build/bin
	@temporary=$$(mktemp build/bin/kryon.XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(PROJECT_CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_INCLUDE) -Ibuild/project/gen build/project/gen/*.c \
		-Wl,--gc-sections -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@

$(BUILD_DIR)/ziran-toolchain.stamp: $(ZIRAN_SOURCES)
	$(MAKE) -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) $(ZIRAN_TOOL_TARGETS)
	mkdir -p $(BUILD_DIR)
	touch $@

# Kryon is an ordinary Ziran library. Platform hosts are linked separately.
$(BUILD_DIR)/libkryon.a: $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/obj
	$(ZI2ZIR_BIN) --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZI2C_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/c $(SOURCE)
	@for module in $(MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c $(BUILD_DIR)/c/$$module.c -o $(BUILD_DIR)/obj/$$module.o || exit 1; \
	done
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

# Go compilation is checked alongside the C archive. Packaging builds that
# only link the C library can request that archive directly.
.PHONY: backends-check
backends-check: $(BUILD_DIR)/libkryon.a
	mkdir -p $(BUILD_DIR)/go
	rm -f $(BUILD_DIR)/go/*.go
	$(ZI2GO_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/go $(SOURCE)
	cd $(BUILD_DIR)/go && GO111MODULE=off go test .

TEST_JOBS ?= 4
TEST ?=
# The tests run the ziran command and link libziran.a; build them first.
ziran-test: project-toolchain
	@$(MAKE) --no-print-directory behavior-tests

# Reuse the suite with instrumented host compilers without rebuilding Ziran.
# The locked packages are fetched once first: parallel tests would otherwise
# fetch the same package at the same time, and offline ones find none. A
# pinned toolchain this builds uses the plain compilers, not instrumented ones.
.PHONY: behavior-tests
behavior-tests:
	@env -u CC -u CXX "$(abspath $(ZIRAN_BUILD_DIR))/bin/ziran" fetch
	@ZIRAN_BIN="$(abspath $(ZIRAN_BUILD_DIR))/bin/ziran" \
		ZIRAN_LIB="$(abspath $(ZIRAN_BUILD_DIR))/libziran.a" \
		ZIRAN_INCLUDE="$(abspath $(ZIRAN_DIR))/include" \
		python3 tools/run-ziran-tests.py --jobs "$(TEST_JOBS)" --filter "$(TEST)"

# Run one behavior area without regenerating every UI module and target.
test-focus:
	@test -n "$(TEST)" || { echo 'usage: make test-focus TEST=link_widget' >&2; exit 2; }
	@$(MAKE) --no-print-directory ziran-test TEST="$(TEST)" TEST_JOBS="$(TEST_JOBS)"

header-check:
	sh tools/check-zi-header-free.sh

project-test: project-toolchain build/bin/kryon
	mkdir -p build/project
	$(ZIRAN_DIR)/build/bin/ziran bundle --root tests --module-path src/project \
		--entry project_options_test:ProjectOptionsTest \
		-o build/project/options-test.zx tests/project_options_test.zi
	test "$$($(ZIRAN_DIR)/build/bin/ziran run build/project/options-test.zx)" = 42
	$(ZIRAN_DIR)/build/bin/ziran build --target=c --root tests \
		--module-path src/project -o build/project/c tests/project_options_test.zi
	$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -Ibuild/project/c \
		-c build/project/c/project_options_test.c \
		-o build/project/options-test.o
	$(ZIRAN_DIR)/build/bin/ziran build --target=go --root tests \
		--module-path src/project -o build/project/go tests/project_options_test.zi
	cd build/project/go && GO111MODULE=off go test .
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/terminal_project_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_static_archive_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_host_test.py
	@$(MAKE) --no-print-directory templates-test

.PHONY: renderer-artifact-test
renderer-artifact-test: build/bin/kryon
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/renderer_artifact_test.py

.PHONY: zx-test zx-reload-test
include mk/zx-canvas.mk
include mk/raylib-library.mk
include mk/zx-raylib.mk
$(ZX_CANVAS_GENERATED)/.complete $(ZX_RAYLIB_GENERATED)/.complete: | project-toolchain
zx-reload-test: zx-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_reload_test.py

zx-test: build/bin/kryon zx-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_project_test.py

test: zx-test zx-reload-test

.PHONY: zx-services-test zx-hosted-test zx-headless-test
zx-headless-test: zx-raylib
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_headless_test.py
zx-services-test: zx-raylib zx-canvas
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_services_test.py

zx-hosted-test: zx-raylib zx-canvas
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_hosted_test.py

test: zx-services-test zx-hosted-test zx-headless-test

.PHONY: zx-canvas-test zx-raylib-test zx-android-test gallery-zx gallery-zx-test
gallery-zx: project-toolchain
	python3 scripts/export-gallery-zx.py --ziran $(ZIRAN_BIN)

gallery-zx-test: gallery-zx zx-player
	python3 tests/gallery_zx_test.py

zx-canvas-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_canvas_test.py

zx-raylib-test: zx-test zx-raylib
	@python3 tests/zx_native_test.py --player $(ZX_RAYLIB_PROGRAM) \
		--bundle build/zx-test/download\ with\ spaces/app.zx

# This gate builds only; a device test requires an explicit disposable emulator.
zx-android-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_android_test.py

# `kryon new` for every template, built and rendered on the terminal host.
templates-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) \
		python3 tests/templates_test.py

# Every supported target -- C, Go, and the portable runner --
# renders each template and example on the headless pixmap host, and every
# image must equal C's byte for byte. It needs go and python3, and
# writes build/pixmap-parity/grid.png to look at.
pixmap-parity-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) \
		python3 tests/pixmap_parity_test.py

# Rebuild glyph outlines with the same Ziran tool for both font families.
OUTLINE_FONT_DIR := build/tools/outline-font-c
OUTLINE_FONT_PROGRAM := $(OUTLINE_FONT_DIR)/outline_font
$(OUTLINE_FONT_PROGRAM): tools/outline_font.zi $(ZIRAN_SOURCES) | project-toolchain
	mkdir -p $(OUTLINE_FONT_DIR)
	$(ZIRAN_BIN) build --target=c --exe --root tools $(ZIRAN_STD_PATH) \
		--entry outline_font:main -o $(OUTLINE_FONT_DIR) tools/outline_font.zi

pixmap-font: $(OUTLINE_FONT_PROGRAM)
	printf '%s\n' assets/fonts/LiberationSans-Regular.ttf assets/fonts/DejaVuSans.ttf \
		assets/fonts/NotoSansSymbols-Regular.ttf assets/fonts/NotoSansSymbols2-Regular.ttf | \
		$(OUTLINE_FONT_PROGRAM) > build/pixmap_font.zi
	mv build/pixmap_font.zi src/backend/pixmap_font.zi

.PHONY: pixmap-mono-font
pixmap-mono-font: $(OUTLINE_FONT_PROGRAM)
	printf '%s\n' assets/fonts/LiberationMono-Regular.ttf | $(OUTLINE_FONT_PROGRAM) | sed '/^PixmapFontUnits/,$$!d' > build/pixmap_mono_font.zi
	printf '%s\n' '// Generated from assets/fonts/LiberationMono-Regular.ttf by tools/outline_font.zi.' '// Liberation Mono Regular outlines and advances; license LICENSE-Liberation.' '// Rebuild with make pixmap-mono-font.' '' > src/backend/pixmap_mono_font.zi
	cat build/pixmap_mono_font.zi >> src/backend/pixmap_mono_font.zi

# Installs the kryon command. Outside `ziran tool kryon` it forwards project
# commands to ziran, which runs the Kryon each project's lock pins.
PREFIX ?= $(HOME)/.local
install-user: build/bin/kryon zx-player
	mkdir -p $(PREFIX)/bin
	cp build/bin/kryon $(PREFIX)/bin/kryon.new
	mv -f $(PREFIX)/bin/kryon.new $(PREFIX)/bin/kryon
	cp build/bin/zx $(PREFIX)/bin/zx.new
	mv -f $(PREFIX)/bin/zx.new $(PREFIX)/bin/zx
	$(RM) $(PREFIX)/bin/zib

# Laws state policy independently of its implementation; `ziran check`
# proves each over its whole listed domain.
LAW_MODULES := $(wildcard src/ui/*_laws.zi)
laws: $(BUILD_DIR)/ziran-toolchain.stamp
	@for module in $(LAW_MODULES); do \
		$(ZIRAN_DIR)/build/bin/ziran check --root src/ui $(ZIRAN_STD_PATH) $$module > /dev/null || exit 1; \
	done
	@echo "Kryon laws proved: $(words $(LAW_MODULES)) modules"
	@ZIRAN_BIN=$(ZIRAN_DIR)/build/bin/ziran sh tests/law_mutation_test.sh

source-check:
	sh tools/check-ziran-source.sh
	python3 tests/style_role_policy_test.py

docs-check:
	python3 scripts/feature-matrix-html.py --check

public-surface-check: $(BUILD_DIR)/ziran-toolchain.stamp
	$(ZI2ZIR_BIN) --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/public/core src/ui/Kryon/module.zi
	$(ZI2ZIR_BIN) --root tests --module-path src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/public/widgets tests/public_widgets.zi

check: all ziran-test header-check docs-check project-test public-surface-check host-keys-test laws
.PHONY: canvas-audio-engine-test
canvas-audio-engine-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_audio_engine_zi_test.sh

.PHONY: dom-spec-test
dom-spec-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/dom_spec_zi_test.sh
	@mkdir -p build/scratch/dom-pixels
	@'$(ZIRAN_BIN)' bundle --root tests --module-path src/ui --module-path src/backend --module-path '$(ZIRAN_DIR)/std' --entry dom_pixels_test:main -o build/scratch/dom-pixels/pixels.zx tests/dom_pixels_test.zi
	@test "$$('$(ZIRAN_BIN)' run build/scratch/dom-pixels/pixels.zx)" = 0

.PHONY: desktop-frame-input-test canvas-frame-input-test
desktop-frame-input-test:
	@python3 tests/host_frame_snapshot_test.py desktop
canvas-frame-input-test:
	@python3 tests/host_frame_snapshot_test.py canvas

.PHONY: canvas-events-test
canvas-events-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_events_zi_test.sh

.PHONY: mini-mode-test
mini-mode-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/mini_embed_zi_test.sh

.PHONY: web-js-boundary-test
web-js-boundary-test:
	@python3 tests/web_js_boundary_test.py

.PHONY: canvas-test
canvas-test: canvas-frame-input-test web-js-boundary-test canvas-events-test mini-mode-test dom-spec-test canvas-audio-engine-test
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_backend_test.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_picker_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/http_transfer_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_download_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_export_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/page_socket_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/browser_viewport_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/canvas_audio_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY EM_CACHE="$(abspath $(BUILD_DIR))/emscripten-cache" \
		sh tests/canvas_text_os_wasm_test.sh

.PHONY: canvas-project-test
canvas-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/canvas_project_test.py

.PHONY: page-route-project-test
page-route-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/page_route_project_test.py

.PHONY: dom-project-test
dom-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/dom_project_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		YUE_DESKTOP_RECOVERY=0 KRYON_DOM_INTERACTION=1 python3 tests/dom_project_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		YUE_DESKTOP_RECOVERY=0 KRYON_DOM_PRESS=1 python3 tests/dom_project_test.py

.PHONY: native-text-test native-accessibility-test bundle-preload-test
native-text-test:
	@sh tests/native_text_test.sh
native-accessibility-test:
	@sh tests/native_accessibility_test.sh
bundle-preload-test:
	@ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran sh tests/ziran_bundle_preload_test.sh
test: native-text-test native-accessibility-test bundle-preload-test

.PHONY: typeface-source-test
typeface-source-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/typeface_source_link_test.sh

.PHONY: raylib-project-test
raylib-project-test: build/bin/kryon
	@python3 tests/raylib_project_test.py

.PHONY: program-title-test
program-title-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/program_title_test.sh

.PHONY: raylib-window-test
raylib-window-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY ZIRAN_ROOT="$(ZIRAN_DIR)" \
		ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_window_test.sh

.PHONY: raylib-typeface-test
raylib-typeface-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_typeface_test.sh

.PHONY: raylib-text-pixels-test
raylib-text-pixels-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" \
		sh tests/raylib_text_pixels_test.sh

.PHONY: raylib-mesh-pixels-test
raylib-mesh-pixels-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" \
		sh tests/raylib_mesh_pixels_test.sh

.PHONY: raylib-frame-input-test
raylib-frame-input-test: project-toolchain $(RAYLIB_A)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" RAYLIB_A="$(RAYLIB_A)" \
		sh tests/raylib_frame_snapshot_test.sh

.PHONY: raylib-frame-pixels-test
raylib-frame-pixels-test: project-toolchain $(RAYLIB_A)
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/android_frame_pixels_test.py --host \
		--raylib "$(RAYLIB_A)" --runtime "$(ZIRAN_BUILD_DIR)/libziran.a"

.PHONY: raylib-secondary-window-test
raylib-secondary-window-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_secondary_window_test.sh

.PHONY: raylib-trace-test
raylib-trace-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_trace_test.sh

.PHONY: raylib-clip-test
raylib-clip-test: $(ZI2C_BIN)
	@ZIRAN_DIR="$(abspath $(ZIRAN_DIR))" ZI2C_BIN="$(ZI2C_BIN)" \
		ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_clip_test.sh

.PHONY: raylib-smooth-shape-test
raylib-smooth-shape-test: $(ZI2C_BIN)
	@ZIRAN_DIR="$(abspath $(ZIRAN_DIR))" ZI2C_BIN="$(ZI2C_BIN)" \
		ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_smooth_shape_test.sh

.PHONY: desktop-project-test
desktop-project-test: build/bin/kryon
	@python3 tests/desktop_project_test.py

.PHONY: desktop-size-test
desktop-size-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/desktop_size_test.py

.PHONY: system-clipboard-linux-test
system-clipboard-linux-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		python3 tests/system_clipboard_linux_test.py


.PHONY: window-test
window-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/window_desktop_test.py

.PHONY: tray-test
tray-test: $(ZI2C_BIN)
	@ZI2C_BIN="$(ZI2C_BIN)" sh tests/tray_test.sh

$(ZIRAN_DIR)/build/bin/ziran:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

# Repeat C behavior tests with AddressSanitizer and UndefinedBehaviorSanitizer.
# The CC wrapper preserves the instrumentation for every compile and link.
SANITIZE_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
.PHONY: sanitize-test
sanitize-test: project-toolchain
	@$(MAKE) --no-print-directory $(BUILD_DIR)/libkryon.a
	mkdir -p build/sanitize
	printf '#!/bin/sh\nexec %s %s "$$@"\n' '$(CC)' '$(SANITIZE_FLAGS)' > build/sanitize/cc
	chmod 755 build/sanitize/cc
	@CC="$(abspath build/sanitize/cc)" \
		ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
		$(MAKE) --no-print-directory behavior-tests

# Directories under build/ that targets and tests write. Anything else there
# is left from one-off experiments: put those in build/scratch/. clean-scratch
# removes every other entry that nothing has written to for a day, so a build
# another session is running keeps its files. Go's module cache is read-only,
# so write permission comes back first.
BUILD_OUTPUTS := ziran project bin tools test examples sanitize raylib-ziran zx-reload-test zx-raylib zx-native-test zx-android-test \
	emscripten-cache android-surface-check text-input-platform-test zx-player zx-test zx-canvas zx-canvas-test gallery-zx \
	accessibility text-shaping performance frame-replay frame-inspect frame-input-replay dom-project
# Keep prior generated output while an installed or running app may use it.
BUILD_OUTPUTS += zib-reload-test zib-raylib zib-native-test zib-android-test \
	zib-player zib-test zib-canvas zib-canvas-test gallery-zib
.PHONY: clean-scratch
clean-scratch:
	@for entry in build/* build/.[!.]*; do \
		test -e "$$entry" || continue; \
		case " $(BUILD_OUTPUTS) " in *" $${entry#build/} "*) continue ;; esac; \
		if test -n "$$(find "$$entry" -newermt '-1 day' -print -quit 2>/dev/null)"; then \
			echo "kept $$entry (written in the last day)"; continue; \
		fi; \
		chmod -R u+w -- "$$entry" 2>/dev/null; \
		if rm -rf -- "$$entry" 2>/dev/null; then echo "removed $$entry"; \
		else echo "could not remove all of $$entry"; fi; \
	done

.PHONY: host-keys-test
host-keys-test:
	sh tests/host_keys_test.sh

# Rebuilds the pixmap host's color emoji from Noto Color Emoji
# (Debian: fonts-noto-color-emoji).
EMOJI_FONT ?= /usr/share/fonts/truetype/noto/NotoColorEmoji.ttf
pixmap-emoji: project-toolchain
	mkdir -p build/tools
	LDLIBS=-lz $(ZIRAN_DIR)/build/bin/ziran build --target=c --exe --root tools --module-path src/backend \
		$(ZIRAN_STD_PATH) --entry emoji_bitmaps:main -o build/tools/emoji_bitmaps tools/emoji_bitmaps.zi
	echo $(EMOJI_FONT) | build/tools/emoji_bitmaps/emoji_bitmaps > build/pixmap_emoji.zi
	mv build/pixmap_emoji.zi src/backend/pixmap_emoji.zi

pixmap-surface-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY sh tests/pixmap_surface_test.sh

test: check canvas-test canvas-project-test zx-canvas-test zx-raylib-test page-route-project-test dom-project-test typeface-source-test pixmap-surface-test program-title-test raylib-window-test system-clipboard-linux-test

clean:
	rm -rf $(BUILD_DIR)

.PHONY: pixmap-measure-test
pixmap-measure-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY sh tests/pixmap_measure_test.sh
test: pixmap-measure-test

.PHONY: pixmap-density-test
pixmap-density-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/pixmap_density_test.sh
test: pixmap-density-test

.PHONY: pixmap-frame-test event-wake-sdl-test
pixmap-frame-test: project-toolchain
	sh tests/pixmap_frame_test.sh
event-wake-sdl-test: project-toolchain
	sh tests/event_wake_sdl_test.sh
test: pixmap-frame-test event-wake-sdl-test

.PHONY: pixmap-mesh-test
pixmap-mesh-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/pixmap_mesh_test.sh
test: pixmap-mesh-test

.PHONY: pixmap-parallel-test
pixmap-parallel-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_pixmap_parallel_test.sh
test: pixmap-parallel-test

.PHONY: style-cache-test
style-cache-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_style_cache_test.sh
test: style-cache-test

.PHONY: style-cache-benchmark
style-cache-benchmark: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/style_cache_cost.sh

.PHONY: video-test
video-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER sh tests/ziran_video_linux_test.sh

.PHONY: image-decode-test
image-decode-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER sh tests/ziran_image_decode_linux_test.sh
test: image-decode-test

.PHONY: scene-test
scene-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_scene_test.sh
test: scene-test

.PHONY: image-preview-test
image-preview-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/image_preview_zi_test.sh
test: image-preview-test

.PHONY: android-surface-test android-rotation-pixels-test
android-surface-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/android_surface_test.py

android-rotation-pixels-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/android_rotation_pixels_test.sh

test: android-surface-test android-rotation-pixels-test

.PHONY: window-focus-test
window-focus-test:
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER YUE_DESKTOP_RECOVERY=0 python3 tests/window_focus_test.py
test: window-focus-test

# Asset audio uses actual decoders on a private null device and a muted private
# browser. It never connects to the owner's audio session or desktop.
.PHONY: asset-audio-test
asset-audio-test: $(RAYLIB_A)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" RAYLIB_A="$(RAYLIB_A)" \
		ZIRAN_BUILD_DIR="$(ZIRAN_BUILD_DIR)" python3 tests/media_audio_test.py --browser

test: asset-audio-test

.PHONY: document-storage-test
document-storage-test:
	@ZIRAN_ROOT="$(abspath $(ZIRAN_DIR))" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/document_storage_test.py --browser

.PHONY: pcm-audio-test
pcm-audio-test: $(RAYLIB_A)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" RAYLIB_A="$(RAYLIB_A)" \
		ZIRAN_BUILD_DIR="$(ZIRAN_BUILD_DIR)" python3 tests/media_audio_test.py --browser --pcm

test: pcm-audio-test document-storage-test
