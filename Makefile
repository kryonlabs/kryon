.DEFAULT_GOAL := all

.PHONY: site site-gallery site-browser-test
site:
	python3 scripts/build-site.py

site-gallery:
	python3 scripts/build-gallery.py

site-browser-test:
	python3 tests/site_browser_test.py

JS_RUNTIME ?= node
EMCC ?= $(HOME)/emsdk/upstream/emscripten/emcc

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
# The registry Kryon's tests run bundles through (see install-user).
HOST_REGISTRY := $(abspath build/hosts)
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
KRYON_UI_PATHS := $(foreach directory,$(sort $(shell find src/ui -type d)),--module-path $(directory) --module-path kryon=$(directory))
ZIRAN_STD_PATH := --module-path $(ZIRAN_DIR)/std $(KRYON_UI_PATHS)
KRYON_DIR := $(CURDIR)
include mk/codecs.mk
include mk/zx-player.mk
ZIRAN_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.c $(ZIRAN_DIR)/cmd/zi2*/*.c $(ZIRAN_DIR)/cmd/zx2*/*.c \
    $(ZIRAN_DIR)/cmd/zir*/*.h $(ZIRAN_DIR)/cmd/zi2*/*.h $(ZIRAN_DIR)/cmd/zx2*/*.h $(ZIRAN_DIR)/include/*.h \
    $(ZIRAN_DIR)/scripts/* $(ZIRAN_DIR)/std/*.zi $(ZIRAN_DIR)/Makefile)

SOURCE := $(addprefix src/ui/,$(shell cat src/ui/modules.txt))

.PHONY: all backends-check check laws test test-focus ziran-test header-check source-check docs-check public-surface-check clean project-toolchain project-test templates-test pixmap-font pixmap-emoji install-user host-keys-test
all: source-check $(BUILD_DIR)/libkryon.a backends-check

# Project command. Its implementation and platform integration are Ziran.
project-toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) all

# The kryon tool builds its Ziran toolchain only when it is missing, as ziran
# builds a pinned one: a locked toolchain never changes, and a local one is
# rebuilt in its own checkout.
$(ZI2C_BIN):
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) $(ZIRAN_MAKE_FLAGS) all

build/project/gen/cli_linux.c: $(wildcard src/project/*.zi) $(ZI2C_BIN) $(ZIRAN_BIN)
	mkdir -p build/project/gen
	rm -f build/project/gen/*.c build/project/gen/*.h
	$(ZIRAN_BIN) build --target=c --root src/project \
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
$(BUILD_DIR)/libkryon.a: $(SOURCE) src/ui/modules.txt ziran.toml ziran.lock Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/obj
	rm -f $(BUILD_DIR)/ir/*.zir $(BUILD_DIR)/c/*.[ch] $(BUILD_DIR)/obj/*.o
	$(ZIRAN_BIN) ir --project --root src/ui -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZI2C_BIN) --no-main --root $(BUILD_DIR)/ir -o $(BUILD_DIR)/c $(BUILD_DIR)/ir/*.zir
	@for source in $(BUILD_DIR)/c/*.c; do \
		module=$${source##*/}; module=$${module%.c}; \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c "$$source" -o $(BUILD_DIR)/obj/$$module.o || exit 1; \
	done
	$(AR) rcs $@.new $(BUILD_DIR)/obj/*.o
	mv $@.new $@

# Go compilation is checked alongside the C archive. Packaging builds that
# only link the C library can request that archive directly.
.PHONY: backends-check
backends-check: $(BUILD_DIR)/libkryon.a
	mkdir -p $(BUILD_DIR)/go
	rm -f $(BUILD_DIR)/go/*.go
	$(ZI2GO_BIN) --no-main --root $(BUILD_DIR)/ir -o $(BUILD_DIR)/go $(BUILD_DIR)/ir/*.zir
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
test: markdown-editor-test

.PHONY: markdown-editor-test
# Widget behavior without a raster: text measures and draws through the no-op host.
WIDGET_NOOP_BINDINGS = \
	--bind kryon/font_metrics:MeasureGlyphWidth=ziran_widget_noop_host:MeasureGlyphWidth \
	--bind kryon/font_metrics:MeasureGlyphLineHeight=ziran_widget_noop_host:MeasureGlyphLineHeight \
	--bind kryon/raster_text:RasterText=ziran_widget_noop_host:RasterText \
	--bind kryon/raster_text:RasterTextClipped=ziran_widget_noop_host:RasterTextClipped \
	--bind kryon/raster_text:RasterTextRotated=ziran_mesh_noop_host:RasterTextRotated \
	--bind kryon/raster_shape:RasterRoundedRectangle=ziran_widget_noop_host:RasterRoundedRectangle \
	--bind kryon/raster_shape:RasterRoundedRectangleOutline=ziran_widget_noop_host:RasterRoundedRectangleOutline \
	--bind kryon/raster:RasterLine=ziran_widget_noop_host:RasterLine \
	--bind kryon/paint_queue:RasterImage=ziran_widget_noop_host:RasterImage \
	--bind kryon/image_raster:ImageWidth=ziran_widget_noop_host:ImageWidth \
	--bind kryon/image_raster:ImageHeight=ziran_widget_noop_host:ImageHeight
HEADLESS_WIDGET_BINDINGS = $(WIDGET_NOOP_BINDINGS) \
	--bind kryon/raster_triangles:RasterTriangles=ziran_mesh_noop_host:NoopRasterTriangles
MARKDOWN_TEST_BINDINGS = $(WIDGET_NOOP_BINDINGS) \
	--bind kryon/system_clipboard:SystemClipboardOpenHost=system_clipboard_host:SystemClipboardOpenHost \
	--bind kryon/system_clipboard:SystemClipboardByteHost=system_clipboard_host:SystemClipboardByteHost \
	--bind kryon/system_clipboard:SystemClipboardWriteHost=system_clipboard_host:SystemClipboardWriteHost
markdown-editor-test:
	$(ZIRAN_BIN) ir --project --module-path $(CURDIR)/tests --root $(CURDIR)/tests -o $(CURDIR)/build/markdown-editor/ir $(CURDIR)/tests/markdown_editor_behavior.zi
	@set -eu; unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER; export HARMONY_DESKTOP_RECOVERY=0; \
	for form in source saved; do \
	  input=$(CURDIR)/tests/markdown_editor_behavior.zi; root=$(CURDIR)/tests; \
	  if test "$$form" = saved; then input=$(CURDIR)/build/markdown-editor/ir/markdown_editor_behavior.zir; root=$(CURDIR)/build/markdown-editor/ir; fi; \
	  $(ZIRAN_BIN) build --project --module-path "$$root" $(MARKDOWN_TEST_BINDINGS) --target=c --root "$$root" --entry markdown_editor_behavior:Main --exe -o build/markdown-editor/$$form-c "$$input"; \
	  build/markdown-editor/$$form-c/markdown_editor_behavior; \
	  $(ZIRAN_BIN) build --project --module-path "$$root" $(MARKDOWN_TEST_BINDINGS) --target=go --pkg main --root "$$root" --entry markdown_editor_behavior:Main --exe -o build/markdown-editor/$$form-go "$$input"; \
	  GO111MODULE=off go run build/markdown-editor/$$form-go/*.go; \
	  $(ZIRAN_BIN) bundle --project --module-path "$$root" $(MARKDOWN_TEST_BINDINGS) --root "$$root" --entry markdown_editor_behavior:Main -o build/markdown-editor/$$form.zx "$$input"; \
	  test "$$($(ZIRAN_BIN) run build/markdown-editor/$$form.zx)" = 0; \
	done
	cmp build/markdown-editor/source.zx build/markdown-editor/saved.zx

behavior-tests: $(HOST_REGISTRY)/kryon-desktop.host
	@env -u CC -u CXX "$(abspath $(ZIRAN_BUILD_DIR))/bin/ziran" fetch
	@ZIRAN_HOST_PATH="$(HOST_REGISTRY)" ZIRAN_BIN="$(abspath $(ZIRAN_BUILD_DIR))/bin/ziran" \
		ZIRAN_LIB="$(abspath $(ZIRAN_BUILD_DIR))/libziran.a" \
		ZIRAN_INCLUDE="$(abspath $(ZIRAN_DIR))/include" \
		python3 tools/run-ziran-tests.py --jobs "$(TEST_JOBS)" --filter "$(TEST)"

# Quick feedback for widget, layout and starter work: the behavior areas that
# change with them, the identity tree, and the starter's pin and build checks.
.PHONY: test-fast
test-fast:
	@$(MAKE) --no-print-directory ziran-test TEST=widget,layout TEST_JOBS="$(TEST_JOBS)"
	@$(MAKE) --no-print-directory widget-tree-test toolchain-pin-test templates-test

# Run one behavior area without regenerating every UI module and target.
test-focus:
	@test -n "$(TEST)" || { echo 'usage: make test-focus TEST=link_widget' >&2; exit 2; }
ifeq ($(TEST),markdown_editor_behavior)
	@$(MAKE) --no-print-directory markdown-editor-test
else
	@$(MAKE) --no-print-directory ziran-test TEST="$(TEST)" TEST_JOBS="$(TEST_JOBS)"
endif

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
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_artifact_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_host_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_entry_test.py
	@$(MAKE) --no-print-directory templates-test

# This native-link regression uses GTK development libraries, supplied by
# the host test shard alongside the desktop renderer's native dependencies.
.PHONY: project-system-links-test
project-system-links-test: project-toolchain build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_system_links_test.py

test: project-system-links-test

.PHONY: renderer-artifact-test renderer-consumer-test
renderer-consumer-test:
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/renderer_consumer_test.py
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_artifact_link_test.py

test: renderer-consumer-test

.PHONY: desktop-codec-artifact-test desktop-codec-encode-artifact-test
desktop-codec-artifact-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/desktop_codec_artifact_test.py

# AVIF compression needs libaom; the default artifact decodes with dav1d only.
desktop-codec-encode-artifact-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/desktop_codec_artifact_test.py --encode

test: desktop-codec-artifact-test

renderer-artifact-test: build/bin/kryon
	env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/renderer_artifact_test.py

.PHONY: zx-test zx-linked-test zx-nested-test zx-reload-test
include mk/zx-canvas.mk
include mk/codecs.mk

$(ZX_CANVAS_GENERATED)/.complete: | project-toolchain
zx-reload-test: zx-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_reload_test.py

zx-test: build/bin/kryon zx-player zx-linked-test zx-nested-test
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_project_test.py

# The optional WebKit island: a real staged-page round-trip through the
# player on a private display; skipped where WebKitGTK is unavailable.
.PHONY: webview-island-test
webview-island-test: zx-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/webview_island_test.py

ZX_LINKED_TEST := build/test/zx-linked
zx-linked-test: zx-player
	mkdir -p $(ZX_LINKED_TEST)
	$(ZIRAN_BIN) bundle --root tests --entry zx_linked_guest:main \
		-o $(ZX_LINKED_TEST)/guest.zx tests/zx_linked_guest.zi
	$(ZIRAN_BIN) build --project --target=c --prune-stale $(PLAYER_TARGET) --root tests \
		-o $(ZX_LINKED_TEST)/c tests/zx_linked_test.zi
	$(CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_INCLUDE) -I$(ZX_LINKED_TEST)/c $(ZX_LINKED_TEST)/c/*.c \
		$(PLAYER_LIBS) -o $(ZX_LINKED_TEST)/launcher
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER \
		-u KRYON_CAPTURE_PATH YUE_DESKTOP_RECOVERY=0 xvfb-run -a \
		$(ZX_LINKED_TEST)/launcher $(ZX_LINKED_TEST)/guest.zx

ZX_NESTED_TEST := build/test/zx-nested
zx-nested-test: zx-player
	mkdir -p $(ZX_NESTED_TEST)/child-assets $(ZX_NESTED_TEST)/shell-assets
	printf 'nested fixture\n' > $(ZX_NESTED_TEST)/child-assets/probe
	ZIRAN_STD=$(abspath $(ZIRAN_DIR))/std $(ZIRAN_BIN) bundle --project --define KRYON_HOSTED_UI \
		--root tests --entry zx_nested_child:main --asset-dir assets=$(ZX_NESTED_TEST)/child-assets \
		-o $(ZX_NESTED_TEST)/shell-assets/child.zx tests/zx_nested_child.zi
	ZIRAN_STD=$(abspath $(ZIRAN_DIR))/std $(ZIRAN_BIN) bundle --project --define KRYON_HOSTED_UI \
		--root tests --entry zx_nested_guest:main --asset-dir assets=$(ZX_NESTED_TEST)/shell-assets \
		-o $(ZX_NESTED_TEST)/shell.zx tests/zx_nested_guest.zi
	$(ZIRAN_BIN) build --project --target=c --prune-stale $(PLAYER_TARGET) --root tests \
		-o $(ZX_NESTED_TEST)/c tests/zx_nested_test.zi
	$(CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_INCLUDE) -I$(ZX_NESTED_TEST)/c $(ZX_NESTED_TEST)/c/*.c \
		$(PLAYER_LIBS) -o $(ZX_NESTED_TEST)/launcher
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER \
		HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 \
		KRYON_APP_DATA=$(abspath $(ZX_NESTED_TEST))/data KRYON_CAPTURE_PATH=$(abspath $(ZX_NESTED_TEST))/frame.png \
		xvfb-run -a $(ZX_NESTED_TEST)/launcher $(abspath $(ZX_NESTED_TEST))/shell.zx

test: zx-reload-test

# window.tsx's bundle runs as native code on a private Xvfb through a private
# host registry: in the desktop player's window through `ziran run`, and as
# zx2native's executable linking the desktop host library.
.PHONY: zx-window-test
zx-window-test: zx-player zx-window-host
	@rm -rf build/test/zx-window
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
		ZIRAN_BIN="$(abspath $(ZIRAN_BIN))" ZIRAN_IDENTITY="$$("$(ZIRAN_BIN)" version --identity)" \
		"$(ZIRAN_BIN)" run --target=c tests/zx_window_test.zi -- \
		"$(abspath build/test/zx-window)" "$(abspath $(PLAYER))" "$(abspath $(WINDOW_HOST))" \
		"$(abspath $(dir $(ZIRAN_BIN))zx2native)"
test: zx-window-test

.PHONY: zx-services-test zx-hosted-test zx-headless-test
zx-headless-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_headless_test.py
zx-services-test: zx-player zx-canvas
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_services_test.py

zx-hosted-test: zx-player zx-canvas
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran \
		python3 tests/zx_hosted_test.py

.PHONY: zx-surface-test
zx-surface-test: project-toolchain
	mkdir -p build/scratch/zx-surface
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --entry hosted_surface:main \
		-o build/scratch/zx-surface/hosted_surface.zx examples/hosted_surface.tsx
	$(ZIRAN_BIN) build --project --target=c --root tests --entry zx_surface_test:main --prune-stale \
		-o build/scratch/zx-surface/host tests/zx_surface_test.zi
	$(CC) -O2 -ffunction-sections -fdata-sections -Wl,--gc-sections \
		-I$(ZIRAN_INCLUDE) -Ibuild/scratch/zx-surface/host build/scratch/zx-surface/host/*.c \
		$(ZIRAN_BUILD_DIR)/libziran.a -lm -lpthread -o build/scratch/zx-surface/test
	build/scratch/zx-surface/test build/scratch/zx-surface/hosted_surface.zx > build/scratch/zx-surface/result || \
		{ cat build/scratch/zx-surface/result; exit 1; }
	cat build/scratch/zx-surface/result
	grep -Fx 'click 1' build/scratch/zx-surface/result
	grep -Fx 'text éx' build/scratch/zx-surface/result
	grep -Fx 'text é' build/scratch/zx-surface/result
	grep -Fx 'surface 280 x 160' build/scratch/zx-surface/result
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_TIMED --entry hosted_surface:main \
		-o build/scratch/zx-surface/timed.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/timed.zx timed > build/scratch/zx-surface/timed-result
	cat build/scratch/zx-surface/timed-result
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_ROWS \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry hosted_surface:main \
		-o build/scratch/zx-surface/rows.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/rows.zx rows > build/scratch/zx-surface/rows-result
	cmp build/scratch/zx-surface/timed-result build/scratch/zx-surface/rows-result
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_PIXELS \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry hosted_surface:main \
		-o build/scratch/zx-surface/pixels.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/pixels.zx pixels > build/scratch/zx-surface/pixels-result
	cmp build/scratch/zx-surface/timed-result build/scratch/zx-surface/pixels-result
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_TIMED \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry hosted_surface:main \
		-o build/scratch/zx-surface/external.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/external.zx timed
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_CLOCK --entry hosted_surface:main \
		-o build/scratch/zx-surface/clocked.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/clocked.zx timed
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_WAKE --entry hosted_surface:main \
		-o build/scratch/zx-surface/wake.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/wake.zx wake
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_WAKE --define SURFACE_SMOKE_ROWS \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry hosted_surface:main \
		-o build/scratch/zx-surface/wake-rows.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/wake-rows.zx wake-rows
	$(ZIRAN_BIN) bundle --project --module-path examples/native --bind-host pixmap --define SURFACE_SMOKE_WAKE --define SURFACE_SMOKE_PIXELS \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry hosted_surface:main \
		-o build/scratch/zx-surface/wake-pixels.zx examples/hosted_surface.tsx
	build/scratch/zx-surface/test build/scratch/zx-surface/wake-pixels.zx wake-pixels

# Kryon's portable halves as shared ZX libraries (docs/BACKENDS.md). Each
# library is built once from its export root, keeping the one implementation
# of its code, and a program loads it at run time with `ziran run
# --module-root build/zx-libraries`. The drawing libraries carry the
# software rasterizer (--bind-host pixmap, as zx-surface-test binds it), so
# they need no host grants. The example is the check: linked against the
# four libraries, it must print the frame hash of its statically linked
# build, where the same modules are compiled in.
ZX_LIBRARIES := build/zx-libraries
ZX_LIBRARIES_TEST := build/test/zx-libraries
ZX_LIBRARY_SOURCES := $(wildcard src/backend/*.zi) $(shell find src/ui -name '*.zi') \
	$(wildcard $(ZIRAN_DIR)/std/*.zi) ziran.toml ziran.lock $(ZIRAN_BIN)
# The linked example finds Kryon's modules by plain name, as the libraries
# do under --project. A `kryon=DIR` package path renames the modules it
# supplies (session becomes p<hash>_session), and a record a library hands
# over by value is matched by its module-qualified name.
ZX_LIBRARY_PATHS := $(foreach directory,$(sort $(shell find src/ui -type d)),--module-path $(directory)) \
	--module-path src/backend
ZX_LIBRARY_REQUIRES := --require-module KryonPixmap.v1=kryon-pixmap \
	--require-module KryonSession.v1=kryon-session \
	--require-module KryonTree.v1=kryon-tree \
	--require-module KryonText.v1=kryon-text
# Every call the linked example makes: its foreign Lib<Name> here, the
# library's export there.
ZX_LIBRARY_IMPORTS := \
	$(foreach function,Background Panel Round Mix,--import zx_libraries:Lib$(function)=KryonPixmap.v1:$(function)) \
	$(foreach function,SessionOpen SessionValid SessionIndex SessionClose WidgetPartKey,--import zx_libraries:Lib$(function)=KryonSession.v1:$(function)) \
	$(foreach function,DrawButton TreeIdentityHash FrameStatusMessage,--import zx_libraries:Lib$(function)=KryonTree.v1:$(function)) \
	$(foreach function,DrawLabel TextSpan,--import zx_libraries:Lib$(function)=KryonText.v1:$(function))

.PHONY: zx-libraries zx-libraries-example
zx-libraries: $(ZX_LIBRARIES)/kryon-pixmap.zx $(ZX_LIBRARIES)/kryon-session.zx \
	$(ZX_LIBRARIES)/kryon-tree.zx $(ZX_LIBRARIES)/kryon-text.zx zx-libraries-example

$(ZX_LIBRARIES)/kryon-pixmap.zx: src/backend/pixmap_library.zi $(ZX_LIBRARY_SOURCES)
	mkdir -p $(ZX_LIBRARIES)
	$(ZIRAN_BIN) bundle --project --library kryon-pixmap --bind-host pixmap \
		--export pixmap_library:Background --export pixmap_library:Panel \
		--export pixmap_library:Round --export pixmap_library:Mix \
		-o $@ src/backend/pixmap_library.zi

$(ZX_LIBRARIES)/kryon-session.zx: src/backend/session_library.zi $(ZX_LIBRARY_SOURCES)
	mkdir -p $(ZX_LIBRARIES)
	$(ZIRAN_BIN) bundle --project --library kryon-session \
		--export session_library:SessionOpen --export session_library:SessionValid \
		--export session_library:SessionIndex --export session_library:SessionClose \
		--export session_library:WidgetPartKey \
		-o $@ src/backend/session_library.zi

$(ZX_LIBRARIES)/kryon-tree.zx: src/backend/tree_library.zi $(ZX_LIBRARY_SOURCES)
	mkdir -p $(ZX_LIBRARIES)
	$(ZIRAN_BIN) bundle --project --library kryon-tree --bind-host pixmap \
		--export tree_library:DrawButton --export tree_library:TreeIdentityHash \
		--export tree_library:FrameStatusMessage \
		-o $@ src/backend/tree_library.zi

$(ZX_LIBRARIES)/kryon-text.zx: src/backend/text_library.zi $(ZX_LIBRARY_SOURCES)
	mkdir -p $(ZX_LIBRARIES)
	$(ZIRAN_BIN) bundle --project --library kryon-text --bind-host pixmap \
		--export text_library:DrawLabel --export text_library:TextSpan \
		-o $@ src/backend/text_library.zi

# The example both ways: statically linked, with the four modules compiled
# in, and linked, with them loaded from --module-root. Each prints one frame
# hash, and the two outputs must agree.
zx-libraries-example: $(ZX_LIBRARIES)/kryon-pixmap.zx $(ZX_LIBRARIES)/kryon-session.zx \
	$(ZX_LIBRARIES)/kryon-tree.zx $(ZX_LIBRARIES)/kryon-text.zx examples/zx_libraries.zi \
	$(ZX_LIBRARY_SOURCES)
	mkdir -p $(ZX_LIBRARIES_TEST)
	$(ZIRAN_BIN) bundle --project --bind-host pixmap --entry zx_libraries:main \
		-o $(ZX_LIBRARIES_TEST)/static.zx examples/zx_libraries.zi
	$(ZIRAN_BIN) bundle --root examples --module-path $(ZIRAN_DIR)/std $(ZX_LIBRARY_PATHS) \
		--define KRYON_ZX_LIBRARIES $(ZX_LIBRARY_REQUIRES) $(ZX_LIBRARY_IMPORTS) \
		--entry zx_libraries:main -o $(ZX_LIBRARIES_TEST)/linked.zx examples/zx_libraries.zi
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 $(ZIRAN_BIN) run $(ZX_LIBRARIES_TEST)/static.zx \
		> $(ZX_LIBRARIES_TEST)/static.out
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 $(ZIRAN_BIN) run --module-root $(ZX_LIBRARIES) \
		$(ZX_LIBRARIES_TEST)/linked.zx > $(ZX_LIBRARIES_TEST)/linked.out
	cmp $(ZX_LIBRARIES_TEST)/static.out $(ZX_LIBRARIES_TEST)/linked.out
	cat $(ZX_LIBRARIES_TEST)/linked.out

test: zx-libraries-example

.PHONY: surface-render-test
surface-render-test: project-toolchain
	@set -e; for name in window_chrome_test workspace_pager_test zx_surface_damage_test zx_surface_wheel_test; do \
		out=build/scratch/$$name; mkdir -p "$$out"; \
		$(ZIRAN_BIN) build --project --target=c --root tests --bind-host pixmap \
			--entry $$name:main --exe -o "$$out/c" "tests/$$name.zi"; \
		"$$out/c/$$name"; \
		$(ZIRAN_BIN) bundle --project --root tests --bind-host pixmap \
			--entry $$name:main -o "$$out/test.zx" "tests/$$name.zi"; \
		$(ZIRAN_BIN) run "$$out/test.zx" > "$$out/result"; \
		cat "$$out/result"; test "$$(tail -1 "$$out/result")" = 0; \
	done
	$(ZIRAN_BIN) build --project --target=c --root tests --bind-host pixmap \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry zx_surface_damage_test:main --exe \
		-o build/scratch/zx_surface_damage_test/external-c tests/zx_surface_damage_test.zi
	build/scratch/zx_surface_damage_test/external-c/zx_surface_damage_test
	$(ZIRAN_BIN) bundle --project --root tests --bind-host pixmap \
		--define KRYON_PIXMAP_EXTERNAL_BUFFER --entry zx_surface_damage_test:main \
		-o build/scratch/zx_surface_damage_test/external.zx tests/zx_surface_damage_test.zi
	$(ZIRAN_BIN) run build/scratch/zx_surface_damage_test/external.zx > build/scratch/zx_surface_damage_test/external-result
	cmp build/scratch/zx_surface_damage_test/result build/scratch/zx_surface_damage_test/external-result

test: zx-services-test zx-hosted-test zx-headless-test zx-surface-test surface-render-test

.PHONY: zx-canvas-test zx-android-test gallery-zx gallery-zx-test
gallery-zx: project-toolchain
	python3 scripts/export-gallery-zx.py --ziran $(ZIRAN_BIN)

gallery-zx-test: gallery-zx zx-player
	python3 tests/gallery_zx_test.py

zx-canvas-test: build/bin/kryon zx-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_canvas_test.py

.PHONY: zx-native-test
zx-native-test: zx-test zx-player
	@python3 tests/zx_native_test.py --bundle build/zx-test/download\ with\ spaces/app.zx

test: zx-native-test


# This gate builds only; a device test requires an explicit disposable emulator.
zx-android-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zx_android_test.py

# `kryon new` for every template, built and rendered on the terminal host.
templates-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) \
		python3 tests/templates_test.py

# `kryon run desktop --png` renders a whole app offscreen at any size: the
# dashboard example at two sizes, then restyled.
.PHONY: shot-test
shot-test:
	@rm -rf build/test/shot
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS ZIRAN_BIN="$(ZIRAN_BIN)" \
		"$(ZIRAN_BIN)" run --target=c --root tests tests/shot_test.zi
test: shot-test

# kryon init pins a floating [toolchain] to the Ziran Kryon's templates pin.
.PHONY: toolchain-pin-test
toolchain-pin-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY "$(ZIRAN_BIN)" run --target=c --root tests \
		--module-path src/project tests/toolchain_pin_test.zi
test: toolchain-pin-test

# Every file block in docs/CHEATSHEET.md builds, shoots and passes its own
# driver test on a private display.
.PHONY: cheatsheet-test
cheatsheet-test:
	@rm -rf build/test/cheatsheet
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS ZIRAN_BIN="$(ZIRAN_BIN)" \
		"$(ZIRAN_BIN)" run --target=c --root tests tests/cheatsheet_test.zi
test: cheatsheet-test

# kryon run --watch rebuilds and relaunches the app after an edit, on a
# private display.
.PHONY: watch-test
watch-test:
	@rm -rf build/test/watch
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 ZIRAN_BIN="$(ZIRAN_BIN)" \
		"$(ZIRAN_BIN)" run --target=c tests/watch_test.zi
test: watch-test

# The test driver names the fix when no app answers.
.PHONY: driver-messages-test
driver-messages-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY "$(ZIRAN_BIN)" run --target=c tests/driver_messages_test.zi
test: driver-messages-test

# Chip, ListItem and Skeleton shot light and dark and driven on a private display.
.PHONY: new-widgets-test
new-widgets-test:
	@rm -rf build/test/new-widgets
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 ZIRAN_BIN="$(ZIRAN_BIN)" \
		"$(ZIRAN_BIN)" run --target=c tests/new_widgets_test.zi
test: new-widgets-test

# The starter's binary size and the work of a 1,000-row list stay within
# their recorded budgets: counts, never timings.
.PHONY: budget-test
budget-test: build/bin/kryon
	@rm -rf build/test/budget build/test/budget-c
	$(ZIRAN_BIN) build --project --module-path $(CURDIR)/tests $(HEADLESS_WIDGET_BINDINGS) --target=c \
		--root $(CURDIR)/tests --entry budget_test:Main --exe -o build/test/budget-c $(CURDIR)/tests/budget_test.zi
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_BIN="$(ZIRAN_BIN)" build/test/budget-c/budget_test
test: budget-test

# Unkeyed widgets keep their identity by call site; placed widgets fill
# their column and take input where they were placed.
.PHONY: widget-tree-test
widget-tree-test:
	@rm -rf build/test/widget-tree-c
	$(ZIRAN_BIN) build --project --module-path $(CURDIR)/tests $(HEADLESS_WIDGET_BINDINGS) --target=c \
		--root $(CURDIR)/tests --entry widget_tree_test:Main --exe -o build/test/widget-tree-c $(CURDIR)/tests/widget_tree_test.zi
	@build/test/widget-tree-c/widget_tree_test
test: widget-tree-test

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
# A registry holding this checkout's player for the toolchain that built it:
# `ziran run` hands a bundle only to a provider registered with its own
# toolchain identity, so tests run bundles through this one, not whatever
# player happens to be installed.
$(HOST_REGISTRY)/kryon-desktop.host: $(PLAYER) $(ZIRAN_BIN)
	@mkdir -p "$(HOST_REGISTRY)"
	@printf '%s\nziran %s\n' "$(abspath $(PLAYER))" "$$("$(ZIRAN_BIN)" version --identity)" > "$@.new"
	@mv -f "$@.new" "$@"

PREFIX ?= $(HOME)/.local
install-user: build/bin/kryon zx-player zx-window-host
	mkdir -p "$(PREFIX)/bin"
	cp build/bin/kryon "$(PREFIX)/bin/kryon.new"
	mv -f "$(PREFIX)/bin/kryon.new" "$(PREFIX)/bin/kryon"
	@# `zx` is Ziran's generic runner; this provider serves `ziran run` and stays off PATH.
	mkdir -p "$(PREFIX)/lib/kryon"
	cp $(PLAYER) "$(PREFIX)/lib/kryon/zx-desktop.new"
	mv -f "$(PREFIX)/lib/kryon/zx-desktop.new" "$(PREFIX)/lib/kryon/zx-desktop"
	mkdir -p "$(PREFIX)/lib/kryon/webview-assets"
	cp $(KRYON_DIR)/scripts/island.py "$(PREFIX)/lib/kryon/kryon-island.py.new"
	mv -f "$(PREFIX)/lib/kryon/kryon-island.py.new" "$(PREFIX)/lib/kryon/kryon-island.py"
	cp $(KRYON_DIR)/assets/webview/bridge.js $(KRYON_DIR)/assets/webview/island.css \
		$(KRYON_DIR)/assets/webview/scene.html $(KRYON_DIR)/assets/webview/scene.js \
		"$(PREFIX)/lib/kryon/webview-assets/"
	cp $(WINDOW_HOST) "$(PREFIX)/lib/kryon/libkryon-desktop.so.new"
	mv -f "$(PREFIX)/lib/kryon/libkryon-desktop.so.new" "$(PREFIX)/lib/kryon/libkryon-desktop.so"
	$(RM) "$(PREFIX)/lib/kryon/libkryon-pixmap.so"
	mkdir -p "$(PREFIX)/share/ziran/hosts"
	@provider=$$(CDPATH= cd -- "$(PREFIX)/lib/kryon" && pwd -P) && \
		identity=$$("$(ZIRAN_BIN)" version --identity) && \
		printf '%s/zx-desktop\nziran %s\n' "$$provider" "$$identity" > "$(PREFIX)/share/ziran/hosts/kryon-desktop.host.new"
	mv -f "$(PREFIX)/share/ziran/hosts/kryon-desktop.host.new" "$(PREFIX)/share/ziran/hosts/kryon-desktop.host"
	@library=$$(CDPATH= cd -- "$(PREFIX)/lib/kryon" && pwd -P) && \
		printf '%s/libkryon-desktop.so\n' "$$library" > "$(PREFIX)/share/ziran/hosts/kryon-desktop.library.new"
	mv -f "$(PREFIX)/share/ziran/hosts/kryon-desktop.library.new" "$(PREFIX)/share/ziran/hosts/kryon-desktop.library"
	$(RM) "$(PREFIX)/share/ziran/hosts/kryon-pixmap.library"
	$(RM) "$(PREFIX)/bin/zib"

# Laws state policy independently of its implementation; `ziran check`
# proves each over its whole listed domain.
LAW_MODULES := $(shell find src/ui -name '*_laws.zi' | LC_ALL=C sort)
laws: $(BUILD_DIR)/ziran-toolchain.stamp
	@printf '%s\n' $(LAW_MODULES) | xargs -P "$(TEST_JOBS)" -I{} \
		sh -c '$(ZIRAN_DIR)/build/bin/ziran check --root src/ui $(ZIRAN_STD_PATH) "$$1" > /dev/null' sh {}
	@echo "Kryon laws proved: $(words $(LAW_MODULES)) modules"
	@TEST_JOBS="$(TEST_JOBS)" ZIRAN_BIN=$(ZIRAN_DIR)/build/bin/ziran sh tests/law_mutation_test.sh

source-check:
	sh tools/check-ziran-source.sh
	python3 tests/style_role_policy_test.py
	env PYTHONDONTWRITEBYTECODE=1 python3 tests/backend_render_benchmark_test.py
	env PYTHONDONTWRITEBYTECODE=1 python3 tests/gpui_render_benchmark_test.py

docs-check:
	python3 scripts/feature-matrix-html.py --check

public-surface-check: canvas-hosted-import-test $(BUILD_DIR)/ziran-toolchain.stamp
	python3 tests/public_surface_test.py

.PHONY: canvas-hosted-import-test
canvas-hosted-import-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(ZIRAN_DIR) sh tests/canvas_hosted_import_test.sh

check: all ziran-test header-check docs-check project-test public-surface-check host-keys-test laws

# CI runs make test as parallel jobs on separate machines. The hosts shard takes
# every test target not named here, so a new one cannot miss CI.
CI_SHARD_ZX := renderer-consumer-test zx-native-test zx-reload-test zx-window-test zx-services-test zx-hosted-test \
	zx-headless-test zx-surface-test surface-render-test native-text-test native-accessibility-test
CI_SHARD_CHECK := check
CI_SHARD_WEB := canvas-test canvas-project-test program-entry-project-test zx-canvas-test \
	page-route-project-test
.PHONY: test-shard-zx test-shard-check test-shard-web test-shard-hosts
test-shard-zx: $(CI_SHARD_ZX)
test-shard-check: $(CI_SHARD_CHECK)
test-shard-web: $(CI_SHARD_WEB)
test-shard-hosts:
	@rest=; for target in $$($(MAKE) --no-print-directory -pn test 2>/dev/null | sed -n 's/^test: //p' | head -1); do \
		case " $(CI_SHARD_ZX) $(CI_SHARD_CHECK) $(CI_SHARD_WEB) " in *" $$target "*) ;; *) rest="$$rest $$target" ;; esac; \
	done; test -n "$$rest"; $(MAKE) --no-print-directory $$rest

# Minutes, not the half hour of make test: source rules plus the hosts whose
# links a shared backend change can break. Run before pushing; CI runs it first.
.PHONY: push-check
push-check:
	python3 scripts/check-google-free.py --source .
	sh scripts/check-submodule-urls.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
		$(MAKE) --no-print-directory source-check public-surface-check docs-check zx-player desktop-project-test gamepad-test text-hook-hosts-ir-test
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
		$(MAKE) --no-print-directory test-focus TEST=ziran_pixmap_gpu_test

.PHONY: pdf-export-test
pdf-export-test:
	sh tests/pdf_export_test.sh
test: pdf-export-test

.PHONY: retained-motion-test
retained-motion-test:
	"$(ZIRAN_BIN)" run --project --target=c --root tests tests/retained_motion_test.zi
test: retained-motion-test

.PHONY: text-hook-hosts-ir-test
text-hook-hosts-ir-test:
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/text_hook_hosts_ir_test.sh
.PHONY: gamepad-test
gamepad-test:
	python3 tests/desktop_gamepad_test.py
	python3 tests/canvas_gamepad_test.py
test: gamepad-test

# An example's generated C holds only what it imports: an empty window is
# the window, its event loop, clearing and presenting; images bring decoders.
.PHONY: pay-per-import-test
pay-per-import-test:
	@rm -rf build/test/pay-per-import
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_BIN="$(ZIRAN_BIN)" \
		"$(ZIRAN_BIN)" run --target=c --root tests --module-path src/project tests/pay_per_import_test.zi
test: pay-per-import-test

# The desktop host's windows on a private Xvfb with real X input: each
# window's own events and surface, the sideways wheel, key repeat, window
# state, the monitor, embedded pictures and fonts, and the render scale.
.PHONY: desktop-windows-test
desktop-windows-test:
	@rm -rf build/test/desktop-windows
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 \
		LDLIBS="-lXtst" "$(ZIRAN_BIN)" run --target=c tests/desktop_windows_test.zi -- build/test/desktop-windows
test: desktop-windows-test

# The desktop audio mixer, SDL sink and asset audio, with no audio device:
# SDL's disk driver stands in for one.
.PHONY: desktop-audio-test
desktop-audio-test:
	@rm -rf build/test/desktop-audio
	@env -u DISPLAY -u WAYLAND_DISPLAY LDLIBS="-lopus" \
		"$(ZIRAN_BIN)" run --target=c tests/desktop_audio_test.zi -- build/test/desktop-audio
test: desktop-audio-test

# The core MP3 archive, built through std/native/AudioCodecs.cmake.
AUDIO_PACKAGE = $$("$(ZIRAN_BIN)" pkg path audio --offline)
MP3_DIR := $(BUILD_DIR)/mp3
MP3_A := $(MP3_DIR)/libziran_mp3.a
# CMake keeps one source per build folder; a local override or a lock change
# moves the audio package, so a folder configured elsewhere starts over.
CMAKE_SOURCE_FOR = source="$(AUDIO_PACKAGE)/native"; \
	if [ -f $(1)/CMakeCache.txt ] && ! grep -qxF "CMAKE_HOME_DIRECTORY:INTERNAL=$$source" $(1)/CMakeCache.txt; then \
		rm -rf $(1); fi; mkdir -p $(1)
$(MP3_A): ziran.lock
	@$(call CMAKE_SOURCE_FOR,$(MP3_DIR)); \
	cmake -S "$$source" -B $(MP3_DIR) -DCMAKE_BUILD_TYPE=Release \
		-DZIRAN_STDLIB="$(ZIRAN_ROOT)/std" -DAUDIO_CORE_VORBIS=OFF > $(MP3_DIR)/configure.log
	@cmake --build $(MP3_DIR) --target ziran_mp3 > $(MP3_DIR)/build.log

# MPEG audio streamed through the mixer equals std/audio_mp3's own decode of the
# audio package's fixtures, and plays as asset audio, with no audio device.
.PHONY: desktop-audio-mp3-test
desktop-audio-mp3-test: $(MP3_A)
	@rm -rf build/test/desktop-audio-mp3
	@env -u DISPLAY -u WAYLAND_DISPLAY LDLIBS="-L$(MP3_DIR) -lziran_mp3 -lopus -lm" \
		"$(ZIRAN_BIN)" run --target=c tests/desktop_audio_mp3_test.zi -- \
		"$(AUDIO_PACKAGE)/tests/fixtures/mp3" build/test/desktop-audio-mp3
test: desktop-audio-mp3-test

# The core Xiph Vorbis archives, built through std/native/AudioCodecs.cmake.
# libvorbis 1.3.7 asks for CMake 2.8, which CMake 4 configures only with a
# policy floor; earlier CMake ignores the variable.
VORBIS_DIR := $(BUILD_DIR)/vorbis
VORBIS_A := $(VORBIS_DIR)/libvorbisfile.a
$(VORBIS_A): ziran.lock
	@$(call CMAKE_SOURCE_FOR,$(VORBIS_DIR)); \
	cmake -S "$$source" -B $(VORBIS_DIR) -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DZIRAN_STDLIB="$(ZIRAN_ROOT)/std" -DAUDIO_CORE_VORBIS=ON > $(VORBIS_DIR)/configure.log
	@cmake --build $(VORBIS_DIR) --target vorbisfile > $(VORBIS_DIR)/build.log

# Ogg Vorbis streamed through the mixer equals std/vorbis_linux's own decode of
# the audio package's fixtures, and plays as asset audio, with no audio device.
.PHONY: desktop-audio-vorbis-test
desktop-audio-vorbis-test: $(VORBIS_A)
	@rm -rf build/test/desktop-audio-vorbis
	@env -u DISPLAY -u WAYLAND_DISPLAY LDLIBS="-L$(VORBIS_DIR) -lvorbisfile -lvorbis -logg -lopus -lm" \
		"$(ZIRAN_BIN)" run --target=c tests/desktop_audio_vorbis_test.zi -- \
		"$(AUDIO_PACKAGE)/tests/fixtures/vorbis" build/test/desktop-audio-vorbis
test: desktop-audio-vorbis-test

.PHONY: android-audio-test
android-audio-test:
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/android_audio_test.sh

.PHONY: cursor-api-test
cursor-api-test:
	python3 tests/cursor_api_test.py
test: cursor-api-test

.PHONY: canvas-audio-engine-test
canvas-audio-engine-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_audio_engine_zi_test.sh

.PHONY: desktop-frame-input-test canvas-frame-input-test
desktop-frame-input-test:
	@python3 tests/host_frame_snapshot_test.py desktop
canvas-frame-input-test:
	@python3 tests/host_frame_snapshot_test.py canvas

.PHONY: canvas-events-test
canvas-events-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/canvas_app_key_queue_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_events_zi_test.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/password_canvas_browser_test.py

.PHONY: mini-mode-test
mini-mode-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/mini_embed_zi_test.sh

.PHONY: web-js-boundary-test
web-js-boundary-test:
	@python3 tests/web_js_boundary_test.py

.PHONY: page-geometry-test
page-geometry-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/page_geometry_test.sh

.PHONY: canvas-test
canvas-test: canvas-frame-input-test web-js-boundary-test canvas-events-test mini-mode-test canvas-audio-engine-test page-geometry-test
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_backend_test.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_picker_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/http_transfer_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_download_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/file_export_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/page_socket_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/browser_viewport_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/webp_browser_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/canvas_audio_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY EM_CACHE="$(abspath $(BUILD_DIR))/emscripten-cache" \
		sh tests/canvas_text_os_wasm_test.sh "$(dir $(ZIRAN_BIN))" "$(EMCC)"

.PHONY: canvas-project-test
canvas-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/canvas_project_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/web_simd_page_test.py

# A program entry (main plus App.Run) built by the project tool for the page
# and for the desktop.
.PHONY: program-entry-project-test
program-entry-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/program_entry_project_test.py
test: program-entry-project-test

.PHONY: page-route-project-test
page-route-project-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/page_route_project_test.py

.PHONY: native-text-test native-accessibility-test
native-text-test:
	@sh tests/native_text_test.sh
native-accessibility-test:
	@sh tests/native_accessibility_test.sh
test: native-text-test native-accessibility-test

.PHONY: typeface-source-test
typeface-source-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/typeface_source_link_test.sh

.PHONY: program-title-test
program-title-test: $(ZI2C_BIN)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/program_title_test.sh

.PHONY: android-frame-input-test
android-frame-input-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" WEBP_A="$(WEBP_A)" \
		AVIF_A="$(AVIF_A)" AOM_A="$(AOM_A)" sh tests/android_frame_input_test.sh

# Android's fopen, for a process with no running activity.
.PHONY: android-fopen-test
android-fopen-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/android_fopen_test.sh

.PHONY: desktop-project-test
desktop-project-test: build/bin/kryon
	@python3 tests/desktop_project_test.py

.PHONY: desktop-size-test
desktop-size-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 python3 tests/desktop_size_test.py

.PHONY: system-clipboard-linux-test
system-clipboard-linux-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		python3 tests/system_clipboard_linux_test.py

# The portable clipboard and PRIMARY API against a bound fixture host.
.PHONY: system-clipboard-test
system-clipboard-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY ZIRAN_ROOT="$(ZIRAN_DIR)" \
		ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/system_clipboard_test.sh
	@echo 'System clipboard and PRIMARY: fixture host, unavailable PRIMARY and buffer limits passed'
test: system-clipboard-test

.PHONY: mime-clipboard-linux-test
mime-clipboard-linux-test:
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/mime_clipboard_linux_test.py
test: mime-clipboard-linux-test

.PHONY: drag-source-linux-test
drag-source-linux-test:
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/drag_source_linux_test.py
test: drag-source-linux-test

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
BUILD_OUTPUTS := ziran project bin tools test examples sanitize webp-ziran avif-ziran opus zx-reload-test zx-native-test zx-android-test \
	emscripten-cache android-surface-check text-input-platform-test zx-player zx-test zx-canvas zx-canvas-test gallery-zx \
	accessibility text-shaping performance frame-replay frame-inspect frame-input-replay
# Keep prior generated output while an installed or running app may use it.
BUILD_OUTPUTS += zib-reload-test zib-native-test zib-android-test \
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

test: check canvas-test canvas-project-test zx-canvas-test page-route-project-test typeface-source-test pixmap-surface-test program-title-test system-clipboard-linux-test

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

.PHONY: pixmap-text-scale-test
pixmap-text-scale-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/pixmap_text_scale_test.sh
test: pixmap-text-scale-test

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

# Opt-in SDL3 SDK and real GPU-renderer parity on a private display.
.PHONY: sdl3-test
sdl3-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		YUE_DESKTOP_RECOVERY=0 ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/sdl3_test.py

.PHONY: style-cache-benchmark
style-cache-benchmark: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/style_cache_cost.sh

# Android NDK link of the WebP decoder for every ABI; needs ANDROID_NDK.
.PHONY: android-webp-link-test
android-webp-link-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/android_webp_link_test.py \
		--ndk "$(or $(ANDROID_NDK),$(HOME)/Android/Sdk/ndk/28.2.13676358)"

.PHONY: scene-image-material-test

scene-image-material-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 sh tests/scene_image_material_test.sh
test: scene-image-material-test

.PHONY: scene-render-benchmark
scene-render-benchmark: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_render_benchmark.py

# Explicit native GLES tests; portable/core consumers do not import this host.
.PHONY: shader-glsl-test scene-gpu-test
shader-glsl-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/shader_glsl_test.sh
scene-gpu-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_gpu_test.py
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_gpu_test.py --glx
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_gpu_test.py --async-test
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_gpu_test.py --async-test --saved
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_camera_shift_test.py

.PHONY: scene-camera-shift-test
scene-camera-shift-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_camera_shift_test.py

.PHONY: daylight-test
daylight-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 sh tests/daylight_test.sh
test: daylight-test

.PHONY: shader-scene-test
shader-scene-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS HARMONY_DESKTOP_RECOVERY=0 sh tests/shader_scene_test.sh
test: shader-scene-test

.PHONY: shader-host-test shader-android-test image-pixels-test
shader-host-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A) $(AVIF_SO)
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/shader_host_test.sh
# Explicit hardware test: USB Moto required; not part of the headless gate.
shader-android-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/shader_android_test.sh
image-pixels-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/image_pixels_test.sh
test: image-pixels-test

.PHONY: native-image-services-test
native-image-services-test: project-toolchain $(AVIF_SO)
	@sh tests/native_image_services_test.sh
test: native-image-services-test


.PHONY: image-preview-test
image-preview-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/image_preview_zi_test.sh
test: image-preview-test
.PHONY: image-preview-web-test
image-preview-web-test: project-toolchain
	sh tests/image_preview_web_test.sh
test: image-preview-web-test

.PHONY: android-surface-test android-rotation-pixels-test
android-surface-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/android_surface_test.py

android-rotation-pixels-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/android_rotation_pixels_test.sh

test: android-surface-test android-rotation-pixels-test android-frame-input-test

.PHONY: window-focus-test
window-focus-test:
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER YUE_DESKTOP_RECOVERY=0 python3 tests/window_focus_test.py
test: window-focus-test

.PHONY: desktop-pointer-test
desktop-pointer-test:
	sh tests/ziran_desktop_pointer_test.sh
test: desktop-pointer-test

# Asset audio uses actual decoders on a private null device and a muted private
# browser. It never connects to the owner's audio session or desktop.
.PHONY: asset-audio-test
asset-audio-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" \
		ZIRAN_BUILD_DIR="$(ZIRAN_BUILD_DIR)" python3 tests/media_audio_test.py --browser

test: asset-audio-test

.PHONY: document-storage-test
document-storage-test:
	@ZIRAN_ROOT="$(abspath $(ZIRAN_DIR))" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/document_storage_test.py --browser

.PHONY: pcm-audio-test
pcm-audio-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" \
		ZIRAN_BUILD_DIR="$(ZIRAN_BUILD_DIR)" python3 tests/media_audio_test.py --browser --pcm

test: pcm-audio-test document-storage-test

.PHONY: android-notification-jvm-test
android-notification-jvm-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/android_notification_jvm_test.py

test: android-notification-jvm-test

.PHONY: android-notifications-scaffold-test
android-notifications-scaffold-test:
	@python3 tests/android_notifications_scaffold_test.py

test: android-notifications-scaffold-test

.PHONY: android-launch-uri-test
android-launch-uri-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/android_launch_uri_test.py
	@python3 tests/android_system_bars_test.py

test: android-launch-uri-test

# Play Billing SDK compilation and account-bound request protocol, no device.
# Requires an Android SDK/NDK and cached Billing AAR from a gplay build.
.PHONY: play-billing-test
play-billing-test:
	@python3 tests/play_billing_test.py

# Optional extensions on the compiled canvas runner retain the bare defaults.
.PHONY: canvas-app-hooks-test
canvas-app-hooks-test:
	@python3 tests/canvas_app_hooks_test.py
canvas-test: canvas-app-hooks-test

.PHONY: rotated-text-test
rotated-text-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A)
	@sh tests/rotated_text_test.sh
	@sh tests/rotated_text_cairo_test.sh
	@python3 tests/rotated_text_web_test.py
test: rotated-text-test

# Canvas, pixmap and Cairo compare shared Lanczos output against Pillow.
# tests/lanczos_backend_goldens.sh runs all three renderers.
.PHONY: lanczos-goldens-test
lanczos-goldens-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A)
	@python3 tests/canvas_lanczos_test.py
test: lanczos-goldens-test

# The Canvas host keeps every image a scene draws decoded once, in private
# headless Firefox.
.PHONY: canvas-asset-cache-test
canvas-asset-cache-test: project-toolchain
	@python3 tests/canvas_asset_cache_test.py
test: canvas-asset-cache-test

# Benchmark receipt: std/resample's Lanczos against Pillow, Pillow-SIMD,
# libvips and stb_image_resize2 here and on the ARMv7 Moto over ADB.
.PHONY: lanczos-benchmark
lanczos-benchmark: project-toolchain
	@sh tests/lanczos_benchmark.sh

.PHONY: avif-decode-test
avif-decode-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A) $(AVIF_SO)
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/avif_decode_test.sh
test: avif-decode-test

.PHONY: avif-browser-test
avif-browser-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/avif_browser_test.py

.PHONY: android-avif-test
android-avif-test: project-toolchain
	@python3 tests/android_avif_decode_test.py --ndk "$(ANDROID_NDK)" $(if $(ANDROID_SERIAL),--serial "$(ANDROID_SERIAL)")

# Static AVIF strict parity: Linux AArch64 (and with ANDROID_NDK, Android
# arm64-v8a) under qemu-user, Android x86/x86_64 natively. AVIF_CROSS_ROOT holds the cross
# GCC (default: the system root).
.PHONY: avif-cross-test
avif-cross-test: project-toolchain
	@python3 tests/avif_cross_test.py $(if $(AVIF_CROSS_ROOT),--toolchain-root "$(AVIF_CROSS_ROOT)") $(if $(ANDROID_NDK),--ndk "$(ANDROID_NDK)")

# Benchmark receipt: libaom oracle vs dav1d threads/SIMD tiers on this host,
# plus the USB Moto with ANDROID_SERIAL and Firefox with AVIF_BENCHMARK_BROWSER=1.
.PHONY: avif-benchmark
avif-benchmark: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/avif_benchmark.py \
		$(if $(ANDROID_SERIAL),--serial "$(ANDROID_SERIAL)" --ndk "$(or $(ANDROID_NDK),$(HOME)/Android/Sdk/ndk/28.2.13676358)") \
		$(if $(AVIF_BENCHMARK_BROWSER),--browser)

.PHONY: avif-render-test
avif-render-test: project-toolchain $(WEBP_A) $(AVIF_A) $(AOM_A) $(AVIF_SO)
	@sh tests/avif_render_test.sh

.PHONY: bitmap-image-test
bitmap-image-test: project-toolchain
	@ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran sh tests/bitmap_store_test.sh
	@ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran sh tests/bitmap_image_test.sh
	@ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran sh tests/bitmap_host_policy_test.sh
	@ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) ZIRAN_BIN=$(abspath $(ZIRAN_DIR))/build/bin/ziran python3 tests/bitmap_image_browser_test.py
test: bitmap-image-test

.PHONY: native-assets-install-test
native-assets-install-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER -u ICEAUTHORITY YUE_DESKTOP_RECOVERY=0 python3 tests/native_assets_install_test.py
test: native-assets-install-test

.PHONY: gtk-style-linux-test
gtk-style-linux-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER -u ICEAUTHORITY YUE_DESKTOP_RECOVERY=0 python3 tests/gtk_style_linux_test.py
.PHONY: gtk-style-types-test
gtk-style-types-test: project-toolchain
	@mkdir -p build/scratch/gtk-style-types
	$(ZIRAN_BIN) bundle --project --root tests --entry gtk_style_types_test:main \
		-o build/scratch/gtk-style-types/test.zx tests/gtk_style_types_test.zi
	@test "$$($(ZIRAN_BIN) run build/scratch/gtk-style-types/test.zx)" = 0
test: gtk-style-linux-test gtk-style-types-test

.PHONY: desktop-native-test
desktop-native-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 python3 tests/desktop_native_test.py
test: desktop-native-test

.PHONY: gtk-scroll-linux-test
gtk-scroll-linux-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u WAYLAND_SOCKET -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 python3 tests/gtk_scroll_linux_test.py
test: gtk-scroll-linux-test

.PHONY: file-date-linux-test
file-date-linux-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS YUE_DESKTOP_RECOVERY=0 python3 tests/file_date_linux_test.py
test: file-date-linux-test

.PHONY: android-surface-platform-test
android-surface-platform-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS python3 tests/android_surface_platform_test.py
test: android-surface-platform-test

.PHONY: android-source-generation-test
android-source-generation-test: project-toolchain
	@python3 tests/android_source_generation_test.py
test: android-source-generation-test

.PHONY: android-egl-composition-test
android-egl-composition-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/android_egl_composition_test.sh
test: android-egl-composition-test

.PHONY: widget-import-order-test
widget-import-order-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" sh tests/widget_import_order_test.sh
test: widget-import-order-test

.PHONY: toast-action-test
toast-action-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/toast_action_test.py

test: toast-action-test

.PHONY: hosted-widgets-import-test
hosted-widgets-import-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/hosted_widgets_import_test.py
test: hosted-widgets-import-test

.PHONY: window-runtime-test
window-runtime-test: $(ZI2C_BIN)
	ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/window_runtime_test.sh

test: window-runtime-test


# Cross-build and execute the Windows SDL/Cairo host on a private Wine display.
.PHONY: windows-desktop-host-test
BUILD_OUTPUTS += windows-desktop
windows-desktop-host-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		HARMONY_DESKTOP_RECOVERY=0 python3 tests/windows_desktop_test.py

# Owned frame snapshots and desktop file-picture cache services.
.PHONY: cairo-picture-cache-test desktop-frame-texture-test android-frame-texture-test
cairo-picture-cache-test: project-toolchain
	@sh tests/ziran_cairo_picture_cache_test.sh
desktop-frame-texture-test: project-toolchain
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS -u SESSION_MANAGER HARMONY_DESKTOP_RECOVERY=0 YUE_DESKTOP_RECOVERY=0 sh tests/ziran_desktop_frame_texture_test.sh
android-frame-texture-test: project-toolchain
	@sh tests/ziran_android_frame_texture_test.sh
test: cairo-picture-cache-test desktop-frame-texture-test android-frame-texture-test

.PHONY: android-present-damage-test
android-present-damage-test: project-toolchain codec-archives
	@sh tests/android_present_damage_test.sh
test: android-present-damage-test

.PHONY: image-dimensions-host-test
image-dimensions-host-test: project-toolchain
	@sh tests/image_dimensions_host_test.sh
test: image-dimensions-host-test

.PHONY: android-runtime-test pixmap-camera-test android-image-services-test pixmap-typeface-test android-codec-artifact-test android-typefaces-link-test android-typefaces-device-test android-pictures-link-test android-pictures-device-test
.PHONY: android-process-test
android-process-test: project-toolchain
	@sh tests/android_process_test.sh
test: android-process-test

android-runtime-test: project-toolchain
	@sh tests/ziran_android_runtime_test.sh
.PHONY: android-frame-watch-test
android-frame-watch-test: project-toolchain
	@ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/android_frame_watch_test.sh $(if $(ANDROID_TEST_SERIAL),"$(ANDROID_TEST_SERIAL)")
pixmap-camera-test: project-toolchain
	@sh tests/pixmap_camera_test.sh
android-image-services-test: project-toolchain codec-archives
	@sh tests/android_image_services_test.sh
pixmap-typeface-test: project-toolchain
	@sh tests/pixmap_typeface_test.sh
android-codec-artifact-test: project-toolchain
	@python3 tests/android_codec_artifact_test.py
android-typefaces-link-test: project-toolchain
	@python3 tests/android_vm_device_test.py typefaces
android-typefaces-device-test: project-toolchain
	@python3 tests/android_vm_device_test.py typefaces --serial ZE2223BQZT
android-pictures-link-test: project-toolchain
	@python3 tests/android_vm_device_test.py pictures
android-pictures-device-test: project-toolchain
	@python3 tests/android_vm_device_test.py pictures --serial ZE2223BQZT
test: android-runtime-test pixmap-camera-test android-image-services-test pixmap-typeface-test

.PHONY: mirrored-image-test
mirrored-image-test: project-toolchain
	@sh tests/ziran_cairo_mirrored_image_test.sh
	@sh tests/pixmap_mirrored_image_test.sh
test: mirrored-image-test

.PHONY: cairo-mesh-test
cairo-mesh-test: project-toolchain
	@sh tests/ziran_cairo_mesh_test.sh
test: cairo-mesh-test

.PHONY: scene-skin-test scene-gltf-test scene-gltf-gpu-test watercolor-test practice-material-gpu-test image-pixels-resolver-test
scene-skin-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/scene_skin_test.sh
scene-gltf-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/scene_gltf_test.sh
scene-gltf-gpu-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/scene_gltf_gpu_test.py
watercolor-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/watercolor_test.sh
practice-material-gpu-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" python3 tests/practice_material_gpu_test.py
image-pixels-resolver-test: project-toolchain
	@ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/image_pixels_resolver_test.sh
test: scene-skin-test scene-gltf-test scene-gltf-gpu-test watercolor-test practice-material-gpu-test image-pixels-resolver-test

include mk/shader-benchmark.mk
