.DEFAULT_GOAL := all

CC ?= cc
CXX ?= c++
AR ?= ar
BUILD_DIR ?= build/ziran
# Resolve the configured package; CI supplies its already checked-out source.
ZIRAN_DIR ?= $(if $(ZIRAN_ROOT),$(ZIRAN_ROOT),$(shell ziran pkg path ziran))
export ZIRAN_ROOT := $(abspath $(ZIRAN_DIR))
ZIRAN_BUILD_DIR ?= $(abspath $(ZIRAN_DIR)/build)
ZI2ZIR_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2zir
ZI2C_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2c
ZI2CPP_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2cpp
ZI2GO_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2go
ZIRAN_BIN ?= $(ZIRAN_BUILD_DIR)/bin/ziran
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
ZIRAN_STD_PATH := --module-path $(ZIRAN_DIR)/std
KRYON_DIR := $(CURDIR)
include mk/zib-player.mk
ZIRAN_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.c \
    $(ZIRAN_DIR)/cmd/zir*/*.h $(ZIRAN_DIR)/include/*.h \
    $(ZIRAN_DIR)/scripts/* $(ZIRAN_DIR)/std/*.zi $(ZIRAN_DIR)/Makefile)

SOURCE := $(addprefix src/ui/,$(shell cat src/ui/modules.txt))
MODULES := $(basename $(notdir $(SOURCE)))
OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(addsuffix .o,$(MODULES)))
PLAN9_DIR := build/plan9
PLAN9_FILE_LIST := $(PLAN9_DIR)/generated-c-files.txt

.PHONY: all backends-check check laws plan9-c test test-focus ziran-test header-check source-check docs-check public-surface-check clean project-toolchain project-test templates-test pixmap-parity-test pixmap-font pixmap-emoji install-user dom-project-test libdraw-native-plan9-test
all: source-check $(BUILD_DIR)/libkryon.a backends-check

# Project command. Its implementation and platform integration are Ziran.
project-toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) BUILD_DIR=$(ZIRAN_BUILD_DIR) all

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
		$(CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_INCLUDE) -Ibuild/project/gen build/project/gen/*.c \
		-Wl,--gc-sections -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@

$(BUILD_DIR)/ziran-toolchain.stamp: $(ZIRAN_SOURCES)
	$(MAKE) -C $(ZIRAN_DIR) BUILD_DIR=$(ZIRAN_BUILD_DIR) \
		$(ZI2ZIR_BIN) $(ZI2C_BIN) $(ZI2CPP_BIN) $(ZI2GO_BIN)
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

# The library also compiles as C++ and passes its Go tests. Apps that only
# link the C library, and packaging builds without Go, skip this check.
.PHONY: backends-check
backends-check: $(BUILD_DIR)/libkryon.a
	mkdir -p $(BUILD_DIR)/cpp $(BUILD_DIR)/go $(BUILD_DIR)/obj-cpp
	$(ZI2CPP_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/cpp $(SOURCE)
	rm -f $(BUILD_DIR)/go/*.go
	$(ZI2GO_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/go $(SOURCE)
	@for module in $(MODULES); do \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/cpp -c $(BUILD_DIR)/cpp/$$module.cpp -o $(BUILD_DIR)/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/go && GO111MODULE=off go test .

TEST_JOBS ?= 4
TEST ?=
# The tests run the ziran command and link libziran.a; build them first.
ziran-test: project-toolchain
	@$(MAKE) --no-print-directory behavior-tests

# Reuse the suite with instrumented host compilers without rebuilding Ziran.
.PHONY: behavior-tests
behavior-tests:
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
		-o build/project/options-test.zib tests/project_options_test.zi
	test "$$($(ZIRAN_DIR)/build/bin/ziran run build/project/options-test.zib)" = 42
	$(ZIRAN_DIR)/build/bin/ziran build --target=cpp --root tests \
		--module-path src/project -o build/project/cpp tests/project_options_test.zi
	$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -Ibuild/project/cpp \
		-c build/project/cpp/project_options_test.cpp \
		-o build/project/options-test.o
	$(ZIRAN_DIR)/build/bin/ziran build --target=go --root tests \
		--module-path src/project -o build/project/go tests/project_options_test.zi
	cd build/project/go && GO111MODULE=off go test .
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/terminal_project_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_static_archive_test.py
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/project_host_test.py
	@$(MAKE) --no-print-directory templates-test

.PHONY: zib-test zib-reload-test
zib-reload-test: zib-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zib_reload_test.py

zib-test: build/bin/kryon zib-player
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) python3 tests/zib_project_test.py

test: zib-test zib-reload-test

# `kryon new` for every template, built and rendered on the terminal host.
templates-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) \
		python3 tests/templates_test.py

# Every Ziran target -- C, C++, Go, Rust, Python, and the portable runner --
# renders each template and example on the headless pixmap host, and every
# image must equal C's byte for byte. It needs go, cargo, and python3, and
# writes build/pixmap-parity/grid.png to look at.
pixmap-parity-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY ZIRAN_ROOT=$(abspath $(ZIRAN_DIR)) \
		python3 tests/pixmap_parity_test.py

# Rebuilds the pixmap host's glyph outlines from Kryon's font.
pixmap-font: project-toolchain
	mkdir -p build/tools
	$(ZIRAN_DIR)/build/bin/ziran build --target=py --exe --root tools \
		$(ZIRAN_STD_PATH) --entry outline_font:main -o build/tools/outline_font tools/outline_font.zi
	printf '%s\n' assets/fonts/LiberationSans-Regular.ttf assets/fonts/DejaVuSans.ttf \
		assets/fonts/NotoSansSymbols-Regular.ttf assets/fonts/NotoSansSymbols2-Regular.ttf | \
		python3 build/tools/outline_font > build/pixmap_font.zi
	mv build/pixmap_font.zi src/backend/pixmap_font.zi

# Installs the kryon command. Outside `ziran tool kryon` it forwards project
# commands to ziran, which runs the Kryon each project's lock pins.
PREFIX ?= $(HOME)/.local
install-user: build/bin/kryon zib-player
	mkdir -p $(PREFIX)/bin
	cp build/bin/kryon $(PREFIX)/bin/kryon.new
	mv -f $(PREFIX)/bin/kryon.new $(PREFIX)/bin/kryon
	cp build/bin/zib $(PREFIX)/bin/zib.new
	mv -f $(PREFIX)/bin/zib.new $(PREFIX)/bin/zib

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

check: all ziran-test header-check docs-check project-test public-surface-check libdraw-native-plan9-test laws
.PHONY: canvas-audio-engine-test
canvas-audio-engine-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_audio_engine_zi_test.sh

.PHONY: dom-spec-test
dom-spec-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/dom_spec_zi_test.sh

.PHONY: canvas-events-test
canvas-events-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_events_zi_test.sh

.PHONY: web-js-boundary-test
web-js-boundary-test:
	@python3 tests/web_js_boundary_test.py

.PHONY: canvas-test
canvas-test: web-js-boundary-test canvas-events-test dom-spec-test canvas-audio-engine-test
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/canvas_backend_test.sh
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

.PHONY: typeface-source-test
typeface-source-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/typeface_source_link_test.sh

.PHONY: raylib-project-test
raylib-project-test: build/bin/kryon
	@python3 tests/raylib_project_test.py

.PHONY: raylib-window-test
raylib-window-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY ZIRAN_ROOT="$(ZIRAN_DIR)" \
		ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_window_test.sh

.PHONY: raylib-typeface-test
raylib-typeface-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
		ZIRAN_ROOT="$(ZIRAN_DIR)" ZIRAN_BIN="$(ZIRAN_BIN)" sh tests/raylib_typeface_test.sh

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

.PHONY: libdraw-project-test
libdraw-project-test: build/bin/kryon
	@python3 tests/libdraw_project_test.py

.PHONY: window-test
window-test: build/bin/kryon
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/window_desktop_test.py

.PHONY: tray-test
tray-test: $(ZI2C_BIN)
	@ZI2C_BIN="$(ZI2C_BIN)" sh tests/tray_test.sh

$(ZIRAN_DIR)/build/bin/ziran:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

# The behavior tests again, with every C and C++ program they build compiled
# under AddressSanitizer and UndefinedBehaviorSanitizer. The tests call
# $$CC and $$CXX, so wrappers add the flags to each compile and link.
SANITIZE_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
.PHONY: sanitize-test
sanitize-test: project-toolchain
	@$(MAKE) --no-print-directory $(BUILD_DIR)/libkryon.a
	mkdir -p build/sanitize
	printf '#!/bin/sh\nexec %s %s "$$@"\n' '$(CC)' '$(SANITIZE_FLAGS)' > build/sanitize/cc
	printf '#!/bin/sh\nexec %s %s "$$@"\n' '$(CXX)' '$(SANITIZE_FLAGS)' > build/sanitize/c++
	chmod 755 build/sanitize/cc build/sanitize/c++
	@CC="$(abspath build/sanitize/cc)" CXX="$(abspath build/sanitize/c++)" \
		ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
		$(MAKE) --no-print-directory behavior-tests

# Directories under build/ that targets and tests write. Anything else there
# is left from one-off experiments: put those in build/scratch/. clean-scratch
# removes every other entry that nothing has written to for a day, so a build
# another session is running keeps its files. Go's module cache is read-only,
# so write permission comes back first.
BUILD_OUTPUTS := ziran project bin plan9 tools test examples sanitize raylib-ziran zib-reload-test \
	emscripten-cache android-surface-check text-input-platform-test zib-player zib-test
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

.PHONY: plan9-c
plan9-c: source-check $(ZIRAN_DIR)/build/bin/ziran $(BUILD_DIR)/ziran-toolchain.stamp
	rm -rf $(PLAN9_DIR)
	mkdir -p $(PLAN9_DIR)
	$(ZIRAN_BIN) build --target=plan9-c --root src/ui $(ZIRAN_STD_PATH) \
		-o $(PLAN9_DIR) $(SOURCE)
	find $(PLAN9_DIR) -type f -name '*.c' | LC_ALL=C sort > $(PLAN9_FILE_LIST)

.PHONY: libdraw-native-plan9-test
libdraw-native-plan9-test:
	sh tests/host_keys_test.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/libdraw_native_plan9_test.sh

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

test: check canvas-test canvas-project-test page-route-project-test dom-project-test typeface-source-test pixmap-surface-test raylib-window-test system-clipboard-linux-test

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

.PHONY: pixmap-parallel-test
pixmap-parallel-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/ziran_pixmap_parallel_test.sh
test: pixmap-parallel-test

# Requires a canonical Taiji checkout and a private headless QEMU guest.
.PHONY: pixmap-surface-plan9-test
pixmap-surface-plan9-test: project-toolchain
	env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS sh tests/pixmap_surface_plan9_test.sh
