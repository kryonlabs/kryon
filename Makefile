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
ZIRAN_BIN ?= $(ZIRAN_BUILD_DIR)/bin/ziran
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
ZIRAN_STD_PATH := --module-path $(ZIRAN_DIR)/std
ZIRAN_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.c \
    $(ZIRAN_DIR)/cmd/zir*/*.h $(ZIRAN_DIR)/include/*.h \
    $(ZIRAN_DIR)/scripts/* $(ZIRAN_DIR)/std/*.zi $(ZIRAN_DIR)/Makefile)

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
# Files that kss_parser.zi loads into its module; not modules themselves.
KSS_PARTS := $(wildcard src/kss/parser/*.zi)
KSS_MODULES := $(basename $(notdir $(KSS_SOURCE)))
KSS_OBJECTS := $(addprefix $(BUILD_DIR)/kss/obj/,$(addsuffix .o,$(KSS_MODULES)))
SYNTAX_SOURCE := src/syntax/syntax.zi
PLAN9_DIR := build/plan9
PLAN9_FILE_LIST := $(PLAN9_DIR)/generated-c-files.txt

.PHONY: all check laws plan9-c test test-focus ziran-test header-check source-check docs-check public-surface-check clean project-toolchain project-test dom-project-test libdraw-native-plan9-test
all: source-check $(BUILD_DIR)/libkryon.a

.PHONY: plot
plot: $(BUILD_DIR)/libkryon_plot.a

.PHONY: data-views
data-views: $(BUILD_DIR)/libkryon_data_views.a

.PHONY: kss
kss: $(BUILD_DIR)/libkryon_kss.a

.PHONY: syntax
syntax: $(BUILD_DIR)/libkryon_syntax.a

# Project command. Its implementation and platform integration are Ziran.
project-toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

$(ZIRAN_DIR)/build/bin/zi2c:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

build/project/gen/cli_linux.c: src/project/cli_linux.zi src/project/options.zi src/project/compile_commands_linux.zi $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN_DIR)/build/bin/ziran | project-toolchain
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
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/cpp $(BUILD_DIR)/go $(BUILD_DIR)/obj $(BUILD_DIR)/obj-cpp
	$(ZI2ZIR_BIN) --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZI2C_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/c $(SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/cpp $(SOURCE)
	rm -f $(BUILD_DIR)/go/*.go
	$(ZI2GO_BIN) --no-main --root src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/go $(SOURCE)
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
	$(ZI2ZIR_BIN) --root src/plot --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/plot/ir $(PLOT_SOURCE)
	$(ZI2C_BIN) --no-main --root src/plot --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/plot/c $(PLOT_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/plot --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/plot/cpp $(PLOT_SOURCE)
	rm -f $(BUILD_DIR)/plot/go/*.go
	$(ZI2GO_BIN) --no-main --root src/plot --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/plot/go $(PLOT_SOURCE)
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
	$(ZI2ZIR_BIN) --root src/data_views --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/data_views/ir $(DATA_VIEW_SOURCE)
	$(ZI2C_BIN) --no-main --root src/data_views --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/data_views/c $(DATA_VIEW_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/data_views --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/data_views/cpp $(DATA_VIEW_SOURCE)
	rm -f $(BUILD_DIR)/data_views/go/*.go
	$(ZI2GO_BIN) --no-main --root src/data_views --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/data_views/go $(DATA_VIEW_SOURCE)
	@for module in $(DATA_VIEW_MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/data_views/c -c $(BUILD_DIR)/data_views/c/$$module.c -o $(BUILD_DIR)/data_views/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/data_views/cpp -c $(BUILD_DIR)/data_views/cpp/$$module.cpp -o $(BUILD_DIR)/data_views/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/data_views/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(DATA_VIEW_OBJECTS)

# KSS text parsing, formatting, and installation are opt in. Runtime style
# resolution stays in core; core modules never import this package.
$(BUILD_DIR)/libkryon_kss.a: $(KSS_SOURCE) $(KSS_PARTS) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/kss/ir $(BUILD_DIR)/kss/c $(BUILD_DIR)/kss/cpp $(BUILD_DIR)/kss/go $(BUILD_DIR)/kss/obj $(BUILD_DIR)/kss/obj-cpp
	$(ZI2ZIR_BIN) --root src/kss --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/kss/ir $(KSS_SOURCE)
	$(ZI2C_BIN) --no-main --root src/kss --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/kss/c $(KSS_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/kss --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/kss/cpp $(KSS_SOURCE)
	rm -f $(BUILD_DIR)/kss/go/*.go
	$(ZI2GO_BIN) --no-main --root src/kss --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/kss/go $(KSS_SOURCE)
	@for module in $(KSS_MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/kss/c -c $(BUILD_DIR)/kss/c/$$module.c -o $(BUILD_DIR)/kss/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/kss/cpp -c $(BUILD_DIR)/kss/cpp/$$module.cpp -o $(BUILD_DIR)/kss/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/kss/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(KSS_OBJECTS)

# Syntax coloring is optional. Core TextArea paints caller supplied color spans.
$(BUILD_DIR)/libkryon_syntax.a: $(SYNTAX_SOURCE) $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/syntax/ir $(BUILD_DIR)/syntax/c $(BUILD_DIR)/syntax/cpp $(BUILD_DIR)/syntax/go
	$(ZI2ZIR_BIN) --root src/syntax --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/syntax/ir $(SYNTAX_SOURCE)
	$(ZI2C_BIN) --no-main --root src/syntax --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/syntax/c $(SYNTAX_SOURCE)
	$(ZI2CPP_BIN) --no-main --root src/syntax --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/syntax/cpp $(SYNTAX_SOURCE)
	rm -f $(BUILD_DIR)/syntax/go/*.go
	$(ZI2GO_BIN) --no-main --root src/syntax --module-path src/ui $(ZIRAN_STD_PATH) -o $(BUILD_DIR)/syntax/go $(SYNTAX_SOURCE)
	$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/syntax/c -c $(BUILD_DIR)/syntax/c/syntax.c -o $(BUILD_DIR)/syntax/syntax.o
	$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/syntax/cpp -c $(BUILD_DIR)/syntax/cpp/syntax.cpp -o $(BUILD_DIR)/syntax/syntax_cpp.o
	cd $(BUILD_DIR)/syntax/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(BUILD_DIR)/syntax/syntax.o

TEST_JOBS ?= 4
TEST ?=
ziran-test:
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
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/project_optional_packages_test.sh
	@env -u DISPLAY -u WAYLAND_DISPLAY python3 tests/terminal_project_test.py

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
	$(ZI2ZIR_BIN) --root tests --module-path src/plot --module-path src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/public/plot tests/public_plot.zi
	$(ZI2ZIR_BIN) --root tests --module-path src/plot --module-path src/data_views \
		--module-path src/kss --module-path src/syntax --module-path src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/public/optional tests/public_optional.zi

check: all plot data-views kss syntax ziran-test header-check docs-check project-test public-surface-check libdraw-native-plan9-test laws
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

.PHONY: raylib-clip-test
raylib-clip-test: $(ZI2C_BIN)
	@ZI2C_BIN="$(ZI2C_BIN)" sh tests/raylib_clip_test.sh

.PHONY: desktop-project-test
desktop-project-test: build/bin/kryon
	@python3 tests/desktop_project_test.py

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

# Style packs apps can install without the .kss file. Their modules are
# committed, because apps import Kryon as a package without running this
# Makefile; style-pack-check fails when a module no longer matches its pack.
STYLE_PACKS := classic
STYLE_PACK_TOOL := build/tools/style_pack_module

$(STYLE_PACK_TOOL): tools/style_pack_module.zi $(ZIRAN_DIR)/build/bin/ziran
	rm -rf $@-c
	mkdir -p $(dir $@)
	$(ZIRAN_BIN) build --target=c --root tools $(ZIRAN_STD_PATH) \
		--entry style_pack_module:main -o $@-c tools/style_pack_module.zi
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) -I$@-c $@-c/*.c -o $@

.PHONY: style-packs style-pack-check
style-packs: $(STYLE_PACK_TOOL)
	@for pack in $(STYLE_PACKS); do $(STYLE_PACK_TOOL) write $$pack || exit 1; done

style-pack-check: $(STYLE_PACK_TOOL)
	@for pack in $(STYLE_PACKS); do $(STYLE_PACK_TOOL) check $$pack || exit 1; done
	@echo "Style pack modules match: $(STYLE_PACKS)"

# Generated and damaged style sheets through the KSS parser, the rule table,
# and the formatter, built with AddressSanitizer and UndefinedBehaviorSanitizer.
# The same seed makes the same sheets; see tests/kss_fuzz.zi.
FUZZ_SEED ?= 1
FUZZ_COUNT ?= 2000
.PHONY: fuzz-kss
fuzz-kss: $(ZIRAN_DIR)/build/bin/ziran
	@sh tests/kss_fuzz.sh $(FUZZ_SEED) $(FUZZ_COUNT)

.PHONY: plan9-c
plan9-c: source-check $(ZIRAN_DIR)/build/bin/ziran $(BUILD_DIR)/ziran-toolchain.stamp
	rm -rf $(PLAN9_DIR)
	mkdir -p $(PLAN9_DIR)
	$(ZIRAN_BIN) build --target=plan9-c --root src/ui $(ZIRAN_STD_PATH) \
		-o $(PLAN9_DIR) $(SOURCE)
	find $(PLAN9_DIR) -type f -name '*.c' | LC_ALL=C sort > $(PLAN9_FILE_LIST)

.PHONY: libdraw-native-plan9-test
libdraw-native-plan9-test:
	@env -u DISPLAY -u WAYLAND_DISPLAY sh tests/libdraw_native_plan9_test.sh

test: check canvas-test canvas-project-test page-route-project-test dom-project-test typeface-source-test style-pack-check fuzz-kss

clean:
	rm -rf $(BUILD_DIR)
