# Shared build route for a Ziran application configured by ziran.toml.
ifndef PROJECT_NAME
$(error PROJECT_NAME is required; run this through ziran tool Kryon)
endif
ifndef PROJECT_ENTRY
$(error PROJECT_ENTRY is required; run this through ziran tool Kryon)
endif
ifndef KRYON_DIR
$(error KRYON_DIR is required; run this through ziran tool Kryon)
endif
ifndef ZIRAN_DIR
$(error ZIRAN_DIR is required; run this through ziran tool Kryon)
endif
ifndef PROJECT_PROFILE
$(error PROJECT_PROFILE is required; run this through ziran tool Kryon)
endif
ifndef ZIRAN_PACKAGE_ID
$(error ZIRAN_PACKAGE_ID is required; run this through ziran tool Kryon)
endif

ifneq ($(filter $(PROJECT_BACKEND)/$(PROJECT_CODEGEN),terminal/c99 desktop/c99 libdraw/c99 raylib/c99 canvas/c99 dom/c99),$(PROJECT_BACKEND)/$(PROJECT_CODEGEN))
$(error backend $(PROJECT_BACKEND) with codegen $(PROJECT_CODEGEN) is not available)
endif

CC ?= cc
ZIRAN := $(ZIRAN_DIR)/build/bin/ziran
GEN_DIR := build/generated/$(PROJECT_PROFILE)
IR_DIR := $(GEN_DIR)/ir
IR_STAMP := $(IR_DIR)/.complete
C_DIR := $(GEN_DIR)/c
C_STAMP := $(C_DIR)/.complete
PROGRAM := build/$(PROJECT_NAME)-$(PROJECT_PROFILE)
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
PROGRAM := build/$(PROJECT_NAME)-$(PROJECT_PROFILE).html
endif
HOST_MODULE := $(PROJECT_BACKEND)_run
BROWSER_BACKEND := $(filter $(PROJECT_BACKEND),canvas dom)
HOST_ID := $(ZIRAN_PACKAGE_ID)_$(HOST_MODULE)
ifneq ($(BROWSER_BACKEND),)
HOST_ID := $(HOST_MODULE)
endif
PROJECT_CONFIG_FILES := ziran.toml ziran.lock
ZIRAN_MODULE_ARGS := --project
HOST := $(KRYON_DIR)/src/backend/$(HOST_MODULE).zi
APP_SOURCES := $(wildcard src/*.zi src/*/module.zi)
UI_SOURCES := $(wildcard $(KRYON_DIR)/src/ui/*.zi)
PLOT_SOURCES := $(wildcard $(KRYON_DIR)/src/plot/*.zi)
DATA_VIEW_SOURCES := $(wildcard $(KRYON_DIR)/src/data_views/*.zi)
KSS_SOURCES := $(wildcard $(KRYON_DIR)/src/kss/*.zi)
SYNTAX_SOURCES := $(wildcard $(KRYON_DIR)/src/syntax/*.zi)
HOST_SOURCES := $(wildcard $(KRYON_DIR)/src/backend/$(PROJECT_BACKEND)*.zi)
HOST_ROOT_SOURCES := $(HOST)
KRYON_MODULE_PATHS := --module-path $(KRYON_DIR)/src/ui \
	--module-path $(KRYON_DIR)/src/plot \
	--module-path $(KRYON_DIR)/src/data_views \
	--module-path $(KRYON_DIR)/src/kss \
	--module-path $(KRYON_DIR)/src/syntax
ifneq ($(BROWSER_BACKEND),)
HOST_ROOT_SOURCES += $(KRYON_DIR)/src/backend/canvas_raster.zi
IR_COMMAND := $(ZIRAN_DIR)/build/bin/zi2zir --define PLATFORM_WEB \
	--root $(KRYON_DIR)/src/backend --module-path src \
	$(KRYON_MODULE_PATHS) --module-path $(ZIRAN_DIR)/std \
	-o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN_DIR)/build/bin/zi2c --define PLATFORM_WEB \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/*.zir
else
IR_COMMAND := $(ZIRAN) ir $(ZIRAN_MODULE_ARGS) --entry $(HOST_ID):main -o $(IR_DIR) $(HOST_ROOT_SOURCES)
C_COMMAND := $(ZIRAN) build --target=c --entry $(HOST_ID):main \
	--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/$(HOST_ID).zir
endif
ifneq ($(filter $(PROJECT_BACKEND),desktop libdraw),)
HOST_SOURCES += $(KRYON_DIR)/src/backend/cairo_raster.zi
endif
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
CANVAS_LIBS := $(foreach name,window draw input texture text os,--js-library $(KRYON_DIR)/web/canvas_$(name).js)
CANVAS_LIBS += --js-library $(ZIRAN_DIR)/web/ziran_web.js
EMCC ?= /home/wao/emsdk/upstream/emscripten/emcc
endif
ZIRAN_STD_SOURCES := $(wildcard $(ZIRAN_DIR)/std/*.zi)
PROJECT_SOURCE_DEPS := $(PROJECT_CONFIG_FILES) $(PROJECT_ENTRY) $(APP_SOURCES) $(UI_SOURCES) $(PLOT_SOURCES) $(DATA_VIEW_SOURCES) $(KSS_SOURCES) $(SYNTAX_SOURCES) $(HOST_SOURCES) $(ZIRAN_STD_SOURCES)
HOST_DEPS :=
ifeq ($(PROJECT_BACKEND),desktop)
HOST_LIBS := $(shell pkg-config --libs sdl2 cairo)
endif
ifeq ($(PROJECT_BACKEND),libdraw)
PLAN9PORT_DIR ?= $(KRYON_DIR)/../plan9port
HOST_LIBS := -Wl,-E -L$(PLAN9PORT_DIR)/lib -ldraw -lmemdraw -lmux -lthread -l9 -lpthread -ldl $(shell pkg-config --libs cairo)
RUN_ENV := PLAN9=$(PLAN9PORT_DIR) PATH=$(PLAN9PORT_DIR)/bin:$(PATH) DEVDRAW=$(PLAN9PORT_DIR)/bin/devdraw
endif
ifeq ($(PROJECT_BACKEND),raylib)
RAYLIB_SOURCE := $(KRYON_DIR)/vendor/raylib/src
RAYLIB_BUILD := $(KRYON_DIR)/build/raylib-ziran
RAYLIB_A := $(RAYLIB_BUILD)/libraylib.a
RAYLIB_INPUTS := $(wildcard $(RAYLIB_SOURCE)/*.c $(RAYLIB_SOURCE)/*.h $(RAYLIB_SOURCE)/platforms/*.c $(RAYLIB_SOURCE)/Makefile)
RAYLIB_SDL_INCLUDE := $(shell pkg-config --variable=includedir sdl2)
RAYLIB_CFLAGS := $(shell pkg-config --cflags sdl2 libdrm gbm egl glesv2)
RAYLIB_LIBS := $(shell pkg-config --libs sdl2 libdrm gbm egl glesv2)
HOST_DEPS := $(RAYLIB_A)
HOST_LIBS := $(RAYLIB_A) $(RAYLIB_LIBS) -ldl -lpthread
RUN_ENV := KRYON_FONT_PATH=$(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf

$(RAYLIB_A): $(RAYLIB_INPUTS)
	@test -f $(RAYLIB_SOURCE)/raylib.h || { echo "Initialize Kryon's raylib submodule: git -C $(KRYON_DIR) submodule update --init vendor/raylib" >&2; exit 1; }
	mkdir -p $(RAYLIB_BUILD)/source
	cp -R $(RAYLIB_SOURCE)/. $(RAYLIB_BUILD)/source/
	$(MAKE) -j4 -C $(RAYLIB_BUILD)/source \
		RAYLIB_SRC_PATH=. RAYLIB_RELEASE_PATH=.. \
		PLATFORM=PLATFORM_DESKTOP_SDL GRAPHICS=GRAPHICS_API_OPENGL_ES2 \
		RAYLIB_LIBTYPE=STATIC RAYLIB_MODULE_AUDIO=TRUE \
		RAYLIB_MODULE_MODELS=TRUE \
		SDL_INCLUDE_PATH=$(RAYLIB_SDL_INCLUDE) \
		CUSTOM_CFLAGS="-DUSING_SDL2_PROJECT $(RAYLIB_CFLAGS) -O2 -ffunction-sections -fdata-sections"
	@test -f $@
endif
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
EM_CACHE ?= $(KRYON_DIR)/build/emscripten-cache
endif

ifneq ($(PROJECT_LIBRARY),)
HOST_LIBS += -l$(PROJECT_LIBRARY)
endif

.PHONY: run build check install toolchain

# Project builds run the driver, which shells out to zi2zir and zi2c.
toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) \
		build/bin/ziran build/bin/zi2zir build/bin/zi2c

build: toolchain
	$(MAKE) --no-print-directory -f $(KRYON_DIR)/mk/ziran-project.mk $(PROGRAM) \
		PROJECT_NAME=$(PROJECT_NAME) PROJECT_ENTRY=$(PROJECT_ENTRY) \
		PROJECT_BACKEND=$(PROJECT_BACKEND) PROJECT_CODEGEN=$(PROJECT_CODEGEN) \
		PROJECT_PROFILE=$(PROJECT_PROFILE) KRYON_DIR=$(KRYON_DIR) ZIRAN_DIR=$(ZIRAN_DIR)

$(IR_STAMP): $(PROJECT_SOURCE_DEPS) $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(IR_DIR)
	rm -f $(IR_DIR)/*.zir $(IR_STAMP)
	$(IR_COMMAND)
	test -f $(IR_DIR)/$(HOST_ID).zir
	touch $(IR_STAMP)

$(C_STAMP): $(IR_STAMP) $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(C_DIR)
	rm -f $(C_DIR)/*.c $(C_DIR)/*.h $(C_STAMP) $(GEN_DIR)/*.c $(GEN_DIR)/*.h $(GEN_DIR)/.complete
	$(C_COMMAND)
	test -f $(C_DIR)/$(HOST_ID).c
	touch $(C_STAMP)

$(GEN_DIR)/compile_commands.json: $(C_STAMP) $(KRYON_DIR)/tools/write-compile-commands.py
	python3 $(KRYON_DIR)/tools/write-compile-commands.py $(ZIRAN_DIR)/include '$(CC)' $(C_DIR) $(GEN_DIR)/compile_commands.json

$(PROGRAM): $(C_STAMP) $(GEN_DIR)/compile_commands.json $(HOST_DEPS)
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
	@temporary=$$(mktemp build/$(PROJECT_NAME).XXXXXX.html); \
		trap 'rm -f "$$temporary"' EXIT; \
		EM_CACHE=$(EM_CACHE) $(EMCC) -O2 -I$(ZIRAN_DIR)/include -iquote $(C_DIR) \
		$(C_DIR)/*.c $(CANVAS_LIBS) \
		--embed-file $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf \
		-sASYNCIFY -sSINGLE_FILE=1 -sEXIT_RUNTIME=1 -sENVIRONMENT=web \
		--shell-file $(KRYON_DIR)/mk/canvas-shell.html \
		-o "$$temporary" && chmod 644 "$$temporary" && mv "$$temporary" $@
else
	@temporary=$$(mktemp build/$(PROJECT_NAME).XXXXXX.html); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(C_DIR) $(C_DIR)/*.c \
		-Wl,--gc-sections $(HOST_LIBS) -lm -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@
endif

run: build
ifneq ($(filter $(PROJECT_BACKEND),canvas dom),)
	@echo "Kryon browser app: ./$@"
else
	$(RUN_ENV) ./$(PROGRAM)
endif

check: toolchain
	$(ZIRAN) check $(ZIRAN_MODULE_ARGS) $(HOST)

# `ziran install` copies the built program to PREFIX/bin and describes it to
# the desktop. The KRYON_INSTALL_* values come from [tool.Kryon.install] and
# are only ever read as quoted shell variables. The program is replaced by
# rename, so a running copy keeps its file and later builds never touch it.
TERMINAL_APP := false
ifeq ($(PROJECT_BACKEND),terminal)
TERMINAL_APP := true
endif

install: build
ifneq ($(BROWSER_BACKEND),)
	@echo "kryon: browser profiles cannot be installed" >&2; exit 2
else
	@test -n "$$ZIRAN_INSTALL_PREFIX" && test -n "$$ZIRAN_INSTALL_BIN" || \
		{ echo "kryon: run this through ziran install" >&2; exit 2; }
	@set -e; \
	prefix="$$ZIRAN_INSTALL_PREFIX"; bin="$$ZIRAN_INSTALL_BIN"; \
	program="$$prefix/bin/$$bin"; \
	mkdir -p "$$prefix/bin" "$$prefix/share/applications"; \
	cp $(PROGRAM) "$$program.new"; chmod 755 "$$program.new"; \
	mv -f "$$program.new" "$$program"; echo "$$program"; \
	icon=""; \
	if [ -n "$$KRYON_INSTALL_ICON" ]; then \
		mkdir -p "$$prefix/share/pixmaps"; \
		icon="$$prefix/share/pixmaps/$$bin.$${KRYON_INSTALL_ICON##*.}"; \
		cp "$(CURDIR)/$$KRYON_INSTALL_ICON" "$$icon.new"; \
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
	entry "$$program" > "$$desktop.new"; mv -f "$$desktop.new" "$$desktop"; echo "$$desktop"; \
	autostart="$${XDG_CONFIG_HOME:-$$HOME/.config}/autostart/$$bin.desktop"; \
	if [ "$$KRYON_INSTALL_AUTOSTART" = true ]; then \
		mkdir -p "$${autostart%/*}"; \
		launch="$$program"; \
		[ -z "$$KRYON_INSTALL_AUTOSTART_ENV" ] || launch="env $$KRYON_INSTALL_AUTOSTART_ENV $$program"; \
		{ entry "$$launch"; echo "X-GNOME-Autostart-enabled=true"; } > "$$autostart.new"; \
		mv -f "$$autostart.new" "$$autostart"; echo "$$autostart"; \
	elif [ -f "$$autostart" ] && grep -qx "X-Kryon-Installed=true" "$$autostart"; then \
		rm -f "$$autostart"; echo "removed $$autostart"; \
	fi
endif
