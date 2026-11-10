# Build the reusable native player, never the downloaded application's code.
CC ?= cc
ZIRAN := $(ZIRAN_DIR)/build/bin/ziran
PLAYER_DIR := $(KRYON_DIR)/build/zx-player
PLAYER := $(KRYON_DIR)/build/bin/zx
PLAYER_SOURCES := $(KRYON_DIR)/tools/zx.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi) $(shell find $(KRYON_DIR)/src/ui -name '*.zi') $(wildcard $(ZIRAN_DIR)/std/*.zi)

.PHONY: zx-player
zx-player: $(PLAYER)

$(PLAYER): $(PLAYER_SOURCES) $(KRYON_DIR)/mk/zx-player.mk $(KRYON_DIR)/ziran.toml $(KRYON_DIR)/ziran.lock $(ZIRAN_DIR)/build/libziran.a $(ZIRAN)
	mkdir -p $(PLAYER_DIR) $(KRYON_DIR)/build/bin
	cd "$(KRYON_DIR)" && $(ZIRAN) build --project --target=c --prune-stale --define PLATFORM_DESKTOP --root $(KRYON_DIR)/tools \
		-o $(PLAYER_DIR) $(KRYON_DIR)/tools/zx.zi
	@temporary=$$(mktemp $(KRYON_DIR)/build/bin/zx.XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -Wno-main -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(PLAYER_DIR) $(PLAYER_DIR)/*.c \
		$(ZIRAN_DIR)/build/libziran.a -Wl,--gc-sections \
		$$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@
