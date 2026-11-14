.DEFAULT_GOAL := all

ZIRAN_BIN ?= ziran
# Resolve the maintained examples through the ordinary package graph.
KRYON_MODULE_ARGS := --project
# The demos that the desktop hosts import live in demo/, apart from the
# programs here whose names other project modules also use.
DEMO_MODULE_ARGS := --module-path demo
KRYON_SOURCES := $(shell find ../src/ui ../src/backend -type f -name '*.zi' -print)
BUILD_DIR ?= ../build/examples
NATIVE_LIB := ../build/ziran/libkryon.a
PROGRAMS := hello window
NATIVE_PROGRAMS := $(addprefix $(BUILD_DIR)/,$(PROGRAMS))
GO_PROGRAMS := $(addsuffix .go.stamp,$(addprefix $(BUILD_DIR)/matrix/,$(PROGRAMS)))
WEB_PROGRAMS := $(addsuffix .web.stamp,$(addprefix $(BUILD_DIR)/matrix/,$(PROGRAMS)))
ZX_PROGRAMS := $(addsuffix .zx,$(addprefix $(BUILD_DIR)/matrix/,$(PROGRAMS)))

.PHONY: all test backend-matrix desktop desktop-image-test time-picker time-picker-shots clean
all: $(NATIVE_PROGRAMS)

$(NATIVE_LIB): ../Makefile ../src/ui/modules.txt $(KRYON_SOURCES)
	$(MAKE) -C .. all

$(BUILD_DIR)/image.zx: demo/image_demo.zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)
	$(ZIRAN_BIN) bundle --root demo $(KRYON_MODULE_ARGS) \
		--entry image_demo:Frame -o $@ demo/image_demo.zi

$(BUILD_DIR)/image-saved.zx: $(BUILD_DIR)/image.zx
	$(ZIRAN_BIN) ir --root demo $(KRYON_MODULE_ARGS) \
		-o $(BUILD_DIR)/image-ir demo/image_demo.zi
	$(ZIRAN_BIN) bundle --root $(BUILD_DIR)/image-ir \
		--module-path $(BUILD_DIR)/image-ir --entry image_demo:Frame \
		-o $@ $(BUILD_DIR)/image-ir/image_demo.zir
	cmp $(BUILD_DIR)/image.zx $@

# Kryon's ziran.toml [package] links names the desktop host's system
# libraries, so every project C build below links without caller LDLIBS.
$(BUILD_DIR)/image-desktop: desktop_image.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi demo/image_demo.zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/desktop-image-native
	$(ZIRAN_BIN) build --target=c --exe --root . $(KRYON_MODULE_ARGS) $(DEMO_MODULE_ARGS) \
		--entry desktop_image:main -o $(BUILD_DIR)/desktop-image-native desktop_image.zi
	cp $(BUILD_DIR)/desktop-image-native/desktop_image $@

$(BUILD_DIR)/time-picker-desktop: desktop_time_picker.zi demo/time_picker_demo.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/desktop-time-picker-native
	$(ZIRAN_BIN) build --target=c --exe --root . $(KRYON_MODULE_ARGS) $(DEMO_MODULE_ARGS) \
		--entry desktop_time_picker:main -o $(BUILD_DIR)/desktop-time-picker-native desktop_time_picker.zi
	cp $(BUILD_DIR)/desktop-time-picker-native/desktop_time_picker $@

define PROGRAM_RULES
$(BUILD_DIR)/$(1): $(1).zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/$(1)-native
	$(ZIRAN_BIN) build --target=c --exe --root . $(KRYON_MODULE_ARGS) \
		--entry $(1):main -o $(BUILD_DIR)/$(1)-native $(1).zi
	cp $(BUILD_DIR)/$(1)-native/$(1) $$@

$(BUILD_DIR)/matrix/$(1).go.stamp: $(1).zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/matrix/$(1)-go
	$(ZIRAN_BIN) build --target=go --pkg main --exe --root . $(KRYON_MODULE_ARGS) \
		--entry $(1):main -o $(BUILD_DIR)/matrix/$(1)-go $(1).zi
	touch $$@

$(BUILD_DIR)/matrix/$(1).web.stamp: $(1).zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/matrix/$(1)-web
	$(ZIRAN_BIN) build --target=c --target-os web --root . $(KRYON_MODULE_ARGS) \
		--entry $(1):main -o $(BUILD_DIR)/matrix/$(1)-web $(1).zi
	touch $$@

$(BUILD_DIR)/matrix/$(1).zx: $(1).zi $(KRYON_SOURCES) ../ziran.toml Makefile
	mkdir -p $(BUILD_DIR)/matrix
	$(ZIRAN_BIN) bundle --root . $(KRYON_MODULE_ARGS) \
		--entry $(1):main -o $$@ $(1).zi
endef

$(foreach program,$(PROGRAMS),$(eval $(call PROGRAM_RULES,$(program))))

.PHONY: widget-catalog widget-catalog-web
widget-catalog:
	python3 ../scripts/gallery.py
	cd gallery && PROJECT_EXTRA_SOURCES=build/catalog/catalog.zi ziran tool kryon build --profile desktop $(if $(wildcard gallery/ziran.local.toml),,--locked)

widget-catalog-web:
	python3 ../scripts/build-gallery.py

desktop: $(BUILD_DIR)/hello

backend-matrix: $(GO_PROGRAMS) $(WEB_PROGRAMS) $(ZX_PROGRAMS)

test: all backend-matrix

desktop-image-test: $(BUILD_DIR)/image-saved.zx $(BUILD_DIR)/image-desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/image-desktop.png \
		xvfb-run -a $(BUILD_DIR)/image-desktop

# The TimePicker example. `make -C examples time-picker` builds it; run
# ../build/examples/time-picker-desktop from examples/ to open it.
time-picker: $(BUILD_DIR)/time-picker-desktop

# Screenshots of its scenes, each played on a private display and bus.
TIME_PICKER_SCENES := light light-open light-keyboard light-empty dark dark-open dark-empty
TIME_PICKER_SHOTS ?= ../build/time-picker-shots
time-picker-shots: $(BUILD_DIR)/time-picker-desktop
	mkdir -p $(TIME_PICKER_SHOTS)
	for scene in $(TIME_PICKER_SCENES); do \
		env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
			KRYON_FONT_PATH=$(abspath ../assets/fonts/LiberationSans-Regular.ttf) \
			TIME_PICKER_SCENE=$$scene KRYON_CAPTURE_PATH=$(abspath $(TIME_PICKER_SHOTS))/$$scene.png \
			xvfb-run -a -s '-screen 0 1024x768x24' \
			dbus-run-session -- $(BUILD_DIR)/time-picker-desktop || exit 1; \
	done

clean:
	$(RM) -r $(BUILD_DIR)
