ZIRAN_BUILD_DIR ?= $(abspath $(ZIRAN_DIR)/build)
# Build the reusable native player, never the downloaded application's code.
CC ?= cc
PLAYER_HOST_OS ?= $(shell uname -s)
PLAYER_TARGET := --define PLATFORM_DESKTOP
ifeq ($(PLAYER_HOST_OS),Darwin)
PLAYER_LDFLAGS ?= -Wl,-dead_strip
PLAYER_TARGET += --target-os macos --define __APPLE__
PLAYER_PACKAGES := sdl2 cairo freetype2 pangocairo fontconfig glib-2.0
else
PLAYER_LDFLAGS ?= -Wl,--gc-sections
PLAYER_PACKAGES := sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11
endif
ZIRAN := $(ZIRAN_BUILD_DIR)/bin/ziran
# The player links the codec archives; include their build rules so any
# caller of this file (this makefile or another project's) resolves them.
include $(KRYON_DIR)/mk/codecs.mk
PLAYER_HTTP_LIBS := $(or $(shell pkg-config --libs libcurl 2>/dev/null),$(if $(filter Linux,$(shell uname -s)),-l:libcurl.so.4,-lcurl))
PLAYER_LIBS = $(ZIRAN_BUILD_DIR)/libziran.a $(OPUS_A) $(PLAYER_LDFLAGS) \
	$$(pkg-config --libs $(PLAYER_PACKAGES)) $(PLAYER_HTTP_LIBS) -lm -lpthread
PLAYER_DIR := $(KRYON_DIR)/build/zx-player
PLAYER := $(KRYON_DIR)/build/bin/zx
PLAYER_SOURCES := $(KRYON_DIR)/tools/zx.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi) $(shell find $(KRYON_DIR)/src/ui -name '*.zi') $(wildcard $(ZIRAN_DIR)/std/*.zi)

.PHONY: zx-player
zx-player: $(PLAYER) webview-stage

$(PLAYER): $(PLAYER_SOURCES) $(KRYON_DIR)/mk/zx-player.mk $(KRYON_DIR)/ziran.toml $(KRYON_DIR)/ziran.lock $(ZIRAN_BUILD_DIR)/libziran.a $(ZIRAN) $(OPUS_A)
	mkdir -p $(PLAYER_DIR) $(KRYON_DIR)/build/bin
	cd "$(KRYON_DIR)" && $(ZIRAN) build --project --target=c --prune-stale $(PLAYER_TARGET) --root $(KRYON_DIR)/tools \
		-o $(PLAYER_DIR) $(KRYON_DIR)/tools/zx.zi
	@temporary=$$(mktemp $(KRYON_DIR)/build/bin/zx.XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -Wno-main -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(PLAYER_DIR) $(PLAYER_DIR)/*.c \
		$(PLAYER_LIBS) -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@

# The optional island: its owned child script and approved staged pages live
# beside the player executable, with KRYON_WEBVIEW_ASSETS as an override.
.PHONY: webview-stage
webview-stage:
	mkdir -p $(KRYON_DIR)/build/bin/webview-assets
	cp $(KRYON_DIR)/scripts/island.py $(KRYON_DIR)/build/bin/kryon-island.py
	cp $(KRYON_DIR)/assets/webview/bridge.js $(KRYON_DIR)/assets/webview/island.css \
		$(KRYON_DIR)/assets/webview/scene.html $(KRYON_DIR)/assets/webview/scene.js \
		$(KRYON_DIR)/build/bin/webview-assets/

# The desktop host library: Kryon's native SDL2/GPU/Cairo window, frame-loop,
# raster, font, clipboard, theme, clock and service capabilities, which a ZX
# program's native executable (`zx2native`) links by name once install-user
# registers it.
WINDOW_HOST_DIR := $(KRYON_DIR)/build/zx-window-host
WINDOW_HOST := $(KRYON_DIR)/build/lib/libkryon-desktop.so

# Cross-platform native linking needs a PE provider, independently of the
# build machine's ELF provider. Keep its runtime DLLs beside the registration.
.PHONY: windows-window-host install-windows-host
windows-window-host:
	python3 $(KRYON_DIR)/scripts/windows_host.py --ziran "$(ZIRAN_DIR)"

install-windows-host:
	python3 $(KRYON_DIR)/scripts/windows_host.py --ziran "$(ZIRAN_DIR)" --prefix "$(PREFIX)"

.PHONY: zx-window-host
zx-window-host: $(WINDOW_HOST)

$(WINDOW_HOST): $(wildcard $(KRYON_DIR)/src/backend/*.zi) $(shell find $(KRYON_DIR)/src/ui -name '*.zi') $(wildcard $(ZIRAN_DIR)/std/*.zi) $(KRYON_DIR)/mk/zx-player.mk \
	$(KRYON_DIR)/ziran.toml $(KRYON_DIR)/ziran.lock $(ZIRAN)
	mkdir -p $(WINDOW_HOST_DIR) $(KRYON_DIR)/build/lib
	cd "$(KRYON_DIR)" && $(ZIRAN) build --project --target=c --prune-stale --define PLATFORM_DESKTOP \
		--root $(KRYON_DIR)/src/backend -o $(WINDOW_HOST_DIR) $(KRYON_DIR)/src/backend/zx_window_host.zi
	@temporary=$$(mktemp $(KRYON_DIR)/build/lib/libkryon-desktop.XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -O2 -fPIC -shared -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(WINDOW_HOST_DIR) $(WINDOW_HOST_DIR)/*.c -Wl,--gc-sections \
		$$(pkg-config --libs $(PLAYER_PACKAGES)) -lm -lpthread -ldl -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@
