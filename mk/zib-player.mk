# Build the reusable native player, never the downloaded application's code.
CC ?= cc
ZIRAN := $(ZIRAN_DIR)/build/bin/ziran
PLAYER_DIR := $(KRYON_DIR)/build/zib-player
PLAYER := $(KRYON_DIR)/build/bin/zib
PLAYER_SOURCES := $(KRYON_DIR)/tools/zib.zi $(KRYON_DIR)/tools/bundle_watch.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi) $(wildcard $(KRYON_DIR)/src/ui/*.zi) $(wildcard $(ZIRAN_DIR)/std/*.zi)

.PHONY: zib-player
zib-player: $(PLAYER)

$(PLAYER): $(PLAYER_SOURCES) $(KRYON_DIR)/mk/zib-player.mk $(ZIRAN_DIR)/build/libziran.a $(ZIRAN)
	mkdir -p $(PLAYER_DIR) $(KRYON_DIR)/build/bin
	$(ZIRAN) build --target=c --root $(KRYON_DIR)/tools \
		--module-path $(KRYON_DIR)/src/backend --module-path $(KRYON_DIR)/src/ui \
		--module-path $(ZIRAN_DIR)/std -o $(PLAYER_DIR) $(KRYON_DIR)/tools/zib.zi
	@temporary=$$(mktemp $(KRYON_DIR)/build/bin/zib.XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -Wno-main -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(PLAYER_DIR) $(PLAYER_DIR)/*.c \
		$(ZIRAN_DIR)/build/libziran.a -Wl,--gc-sections \
		$$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@
