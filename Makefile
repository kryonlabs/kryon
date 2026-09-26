.DEFAULT_GOAL := all

CC ?= cc
CXX ?= c++
AR ?= ar
BUILD_DIR ?= build/ziran
ZIRAN_DIR ?= ../ziran
ZIRAN_BUILD_DIR ?= $(abspath $(ZIRAN_DIR)/build)
ZI2ZIR_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2zir
ZI2C_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2c
ZI2CPP_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2cpp
ZI2GO_BIN ?= $(ZIRAN_BUILD_DIR)/bin/zi2go
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
ZIRAN_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.c \
    $(ZIRAN_DIR)/cmd/zir*/*.h $(ZIRAN_DIR)/include/*.h \
    $(ZIRAN_DIR)/scripts/* $(ZIRAN_DIR)/Makefile)

SOURCE := $(addprefix src/ui/,$(shell cat src/ui/modules.txt))
MODULES := $(basename $(notdir $(SOURCE)))
OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(addsuffix .o,$(MODULES)))
PLOT_SOURCE := $(wildcard src/plot/*.zi)
PLOT_MODULES := $(basename $(notdir $(PLOT_SOURCE)))
PLOT_OBJECTS := $(addprefix $(BUILD_DIR)/plot/obj/,$(addsuffix .o,$(PLOT_MODULES)))
DATA_VIEW_SOURCE := $(wildcard src/data_views/*.zi)
DATA_VIEW_MODULES := $(basename $(notdir $(DATA_VIEW_SOURCE)))
DATA_VIEW_OBJECTS := $(addprefix $(BUILD_DIR)/data_views/obj/,$(addsuffix .o,$(DATA_VIEW_MODULES)))
KSS_SOURCE := $(wildcard src/kss/*.zi)
KSS_MODULES := $(basename $(notdir $(KSS_SOURCE)))
KSS_OBJECTS := $(addprefix $(BUILD_DIR)/kss/obj/,$(addsuffix .o,$(KSS_MODULES)))
GAME_SOURCE := src/game/raylib_game.zi

.PHONY: all check test test-focus ziran-test header-check source-check clean project-toolchain project-test install-user
all: source-check $(BUILD_DIR)/libkryon.a

.PHONY: plot
plot: $(BUILD_DIR)/libkryon_plot.a

.PHONY: data-views
data-views: $(BUILD_DIR)/libkryon_data_views.a

.PHONY: kss
kss: $(BUILD_DIR)/libkryon_kss.a

.PHONY: raylib-game
raylib-game: $(BUILD_DIR)/libkryon_raylib_game.a

# Project command. Its implementation and platform integration are Ziran.
project-toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

$(ZIRAN_DIR)/build/bin/zi2c:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

build/project/gen/cli_linux.c: src/project/cli_linux.zi src/project/manifest.zi $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN_DIR)/build/bin/ziran | project-toolchain
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

# The user command refreshes its own executable from this checkout on launch.
USER_BIN ?= $(HOME)/.local/bin
install-user: build/bin/kryon
	mkdir -p $(USER_BIN)
	printf '%s\n' '#!/bin/sh' 'set -eu' \
		'make -s -C "$(CURDIR)" build/bin/kryon' \
		'exec "$(CURDIR)/build/bin/kryon" "$$@"' > $(USER_BIN)/kryon
	chmod 755 $(USER_BIN)/kryon

$(BUILD_DIR)/ziran-toolchain.stamp: $(ZIRAN_SOURCES)
	$(MAKE) -C $(ZIRAN_DIR) BUILD_DIR=$(ZIRAN_BUILD_DIR) all
	mkdir -p $(BUILD_DIR)
	touch $@

$(BUILD_DIR)/c/kryon_portable_host.h: tests/support/kryon_portable_host.h
	mkdir -p $(BUILD_DIR)/c
	cp $< $@

$(BUILD_DIR)/libkryon_host.a: tests/support/raster_host.c tests/support/font_metrics_host.c tests/support/image_host.c $(BUILD_DIR)/c/kryon_portable_host.h $(ZIRAN_INCLUDE)/ziran_host.h Makefile
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c tests/support/raster_host.c -o $(BUILD_DIR)/raster_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c tests/support/font_metrics_host.c -o $(BUILD_DIR)/font_metrics_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c tests/support/image_host.c -o $(BUILD_DIR)/image_host.o
	rm -f $@
	$(AR) rcs $@ $(BUILD_DIR)/raster_host.o $(BUILD_DIR)/font_metrics_host.o $(BUILD_DIR)/image_host.o

# Kryon is an ordinary Ziran library. Platform hosts are linked separately.
$(BUILD_DIR)/libkryon.a: $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/cpp $(BUILD_DIR)/go $(BUILD_DIR)/obj $(BUILD_DIR)/obj-cpp
	$(ZI2ZIR_BIN) --root src/ui -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZI2C_BIN) --no-main --root src/ui -o $(BUILD_DIR)/c $(SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/ui -o $(BUILD_DIR)/cpp $(SOURCE)
	rm -f $(BUILD_DIR)/go/*.go
	$(ZI2GO_BIN) --no-main --root src/ui -o $(BUILD_DIR)/go $(SOURCE)
	@for module in $(MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c $(BUILD_DIR)/c/$$module.c -o $(BUILD_DIR)/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/cpp -c $(BUILD_DIR)/cpp/$$module.cpp -o $(BUILD_DIR)/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

# Plot is an opt-in UI package. It depends on libkryon, but core modules do
# not import it. Apps can also import these sources directly through Ziran.
$(BUILD_DIR)/libkryon_plot.a: $(PLOT_SOURCE) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/plot/c $(BUILD_DIR)/plot/cpp $(BUILD_DIR)/plot/go $(BUILD_DIR)/plot/obj $(BUILD_DIR)/plot/obj-cpp
	$(ZI2ZIR_BIN) --root src/plot --module-path src/ui -o $(BUILD_DIR)/plot/ir $(PLOT_SOURCE)
	$(ZI2C_BIN) --no-main --root src/plot --module-path src/ui -o $(BUILD_DIR)/plot/c $(PLOT_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/plot --module-path src/ui -o $(BUILD_DIR)/plot/cpp $(PLOT_SOURCE)
	rm -f $(BUILD_DIR)/plot/go/*.go
	$(ZI2GO_BIN) --no-main --root src/plot --module-path src/ui -o $(BUILD_DIR)/plot/go $(PLOT_SOURCE)
	@for module in $(PLOT_MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/plot/c -c $(BUILD_DIR)/plot/c/$$module.c -o $(BUILD_DIR)/plot/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/plot/cpp -c $(BUILD_DIR)/plot/cpp/$$module.cpp -o $(BUILD_DIR)/plot/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/plot/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(PLOT_OBJECTS)

# TableView and TreeView are optional collection widgets over the core tree.
$(BUILD_DIR)/libkryon_data_views.a: $(DATA_VIEW_SOURCE) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/data_views/c $(BUILD_DIR)/data_views/cpp $(BUILD_DIR)/data_views/go $(BUILD_DIR)/data_views/obj $(BUILD_DIR)/data_views/obj-cpp
	$(ZI2ZIR_BIN) --root src/data_views --module-path src/ui -o $(BUILD_DIR)/data_views/ir $(DATA_VIEW_SOURCE)
	$(ZI2C_BIN) --no-main --root src/data_views --module-path src/ui -o $(BUILD_DIR)/data_views/c $(DATA_VIEW_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/data_views --module-path src/ui -o $(BUILD_DIR)/data_views/cpp $(DATA_VIEW_SOURCE)
	rm -f $(BUILD_DIR)/data_views/go/*.go
	$(ZI2GO_BIN) --no-main --root src/data_views --module-path src/ui -o $(BUILD_DIR)/data_views/go $(DATA_VIEW_SOURCE)
	@for module in $(DATA_VIEW_MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/data_views/c -c $(BUILD_DIR)/data_views/c/$$module.c -o $(BUILD_DIR)/data_views/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/data_views/cpp -c $(BUILD_DIR)/data_views/cpp/$$module.cpp -o $(BUILD_DIR)/data_views/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/data_views/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(DATA_VIEW_OBJECTS)

# KSS text parsing, formatting, and installation are opt in. Runtime style
# resolution stays in core; core modules never import this package.
$(BUILD_DIR)/libkryon_kss.a: $(KSS_SOURCE) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/kss/ir $(BUILD_DIR)/kss/c $(BUILD_DIR)/kss/cpp $(BUILD_DIR)/kss/go $(BUILD_DIR)/kss/obj $(BUILD_DIR)/kss/obj-cpp
	$(ZI2ZIR_BIN) --root src/kss --module-path src/ui -o $(BUILD_DIR)/kss/ir $(KSS_SOURCE)
	$(ZI2C_BIN) --no-main --root src/kss --module-path src/ui -o $(BUILD_DIR)/kss/c $(KSS_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/kss --module-path src/ui -o $(BUILD_DIR)/kss/cpp $(KSS_SOURCE)
	rm -f $(BUILD_DIR)/kss/go/*.go
	$(ZI2GO_BIN) --no-main --root src/kss --module-path src/ui -o $(BUILD_DIR)/kss/go $(KSS_SOURCE)
	@for module in $(KSS_MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/kss/c -c $(BUILD_DIR)/kss/c/$$module.c -o $(BUILD_DIR)/kss/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/kss/cpp -c $(BUILD_DIR)/kss/cpp/$$module.cpp -o $(BUILD_DIR)/kss/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/kss/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(KSS_OBJECTS)

# Raylib's game API is opt in and has a C ABI. Native Go deliberately rejects
# those foreign C symbols; the C and C++ outputs share the same checked source.
$(BUILD_DIR)/libkryon_raylib_game.a: $(GAME_SOURCE) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/game/ir $(BUILD_DIR)/game/c $(BUILD_DIR)/game/cpp
	$(ZI2ZIR_BIN) --root src/game --module-path src/ui --module-path $(ZIRAN_DIR)/std -o $(BUILD_DIR)/game/ir $(GAME_SOURCE)
	$(ZI2C_BIN) --no-main --root src/game --module-path src/ui --module-path $(ZIRAN_DIR)/std -o $(BUILD_DIR)/game/c $(GAME_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/game --module-path src/ui --module-path $(ZIRAN_DIR)/std -o $(BUILD_DIR)/game/cpp $(GAME_SOURCE)
	$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/game/c -c $(BUILD_DIR)/game/c/raylib_game.c -o $(BUILD_DIR)/game/raylib_game.o
	$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/game/cpp -c $(BUILD_DIR)/game/cpp/raylib_game.cpp -o $(BUILD_DIR)/game/raylib_game_cpp.o
	rm -f $@
	$(AR) rcs $@ $(BUILD_DIR)/game/raylib_game.o

TEST_JOBS ?= 4
TEST ?=
ziran-test: $(BUILD_DIR)/libkryon_host.a
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
		--entry project_manifest_test:ProjectManifestTest \
		-o build/project/manifest-test.zib tests/project_manifest_test.zi
	test "$$($(ZIRAN_DIR)/build/bin/ziran run build/project/manifest-test.zib)" = 42
	$(ZIRAN_DIR)/build/bin/ziran build --target=cpp --root tests \
		--module-path src/project -o build/project/cpp tests/project_manifest_test.zi
	$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -Ibuild/project/cpp \
		-c build/project/cpp/project_manifest_test.cpp \
		-o build/project/manifest-test.o
	$(ZIRAN_DIR)/build/bin/ziran build --target=go --root tests \
		--module-path src/project -o build/project/go tests/project_manifest_test.zi
	cd build/project/go && GO111MODULE=off go test .
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/project_optional_packages_test.sh

source-check:
	sh tools/check-ziran-source.sh

check: all plot data-views kss raylib-game ziran-test header-check project-test
.PHONY: typeface-source-test
typeface-source-test: $(ZI2C_BIN)
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/typeface_source_link_test.sh

.PHONY: raylib-project-test
raylib-project-test: build/bin/kryon
	@python3 tests/raylib_project_test.py

test: check typeface-source-test

clean:
	rm -rf $(BUILD_DIR)
