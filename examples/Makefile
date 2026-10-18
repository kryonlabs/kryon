.DEFAULT_GOAL := all

CC ?= cc
ZIRAN_DIR ?= $(if $(ZIRAN_ROOT),$(ZIRAN_ROOT),$(shell ziran pkg path ziran))
ZI2ZIR_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zir)
ZI2ZIB_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zib)
ZI2C_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2c)
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
ZIRAN_STD_PATH := --module-path $(ZIRAN_DIR)/std
BUILD_DIR ?= ../build/examples/ziran
NATIVE_LIB := ../build/ziran/libkryon.a

.PHONY: all test desktop desktop-test desktop-image-test desktop-text-test time-picker time-picker-shots clean
all: $(BUILD_DIR)/hello.svg $(BUILD_DIR)/hello.zib

$(ZI2ZIB_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(ZI2C_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(NATIVE_LIB): ../Makefile ../src/ui/modules.txt $(wildcard ../src/ui/*.zi)
	$(MAKE) -C .. all

$(BUILD_DIR)/hello.zib: hello.zi $(wildcard ../src/ui/*.zi) $(ZI2ZIB_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZI2ZIB_BIN) bundle --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		--entry hello:Frame -o $@ hello.zi

$(BUILD_DIR)/hello-saved.zib: $(BUILD_DIR)/hello.zib
	$(ZI2ZIR_BIN) --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/ir hello.zi
	$(ZI2ZIB_BIN) bundle --root $(BUILD_DIR)/ir \
		--module-path $(BUILD_DIR)/ir --entry hello:Frame \
		-o $@ $(BUILD_DIR)/ir/hello.zir
	cmp $(BUILD_DIR)/hello.zib $@

$(BUILD_DIR)/image.zib: image_demo.zi $(wildcard ../src/ui/*.zi) $(ZI2ZIB_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZI2ZIB_BIN) bundle --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		--entry image_demo:Frame -o $@ image_demo.zi

$(BUILD_DIR)/image-saved.zib: $(BUILD_DIR)/image.zib
	$(ZI2ZIR_BIN) --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/image-ir image_demo.zi
	$(ZI2ZIB_BIN) bundle --root $(BUILD_DIR)/image-ir \
		--module-path $(BUILD_DIR)/image-ir --entry image_demo:Frame \
		-o $@ $(BUILD_DIR)/image-ir/image_demo.zir
	cmp $(BUILD_DIR)/image.zib $@

$(BUILD_DIR)/text.zib: text_hello.zi $(wildcard ../src/ui/*.zi) $(ZI2ZIB_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZI2ZIB_BIN) bundle --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		--entry text_hello:Frame -o $@ text_hello.zi

$(BUILD_DIR)/text-saved.zib: $(BUILD_DIR)/text.zib
	$(ZI2ZIR_BIN) --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/text-ir text_hello.zi
	$(ZI2ZIB_BIN) bundle --root $(BUILD_DIR)/text-ir \
		--module-path $(BUILD_DIR)/text-ir --entry text_hello:Frame \
		-o $@ $(BUILD_DIR)/text-ir/text_hello.zir
	cmp $(BUILD_DIR)/text.zib $@

$(BUILD_DIR)/native/hello_host.c: hello_host.zi hello.zi $(wildcard ../src/ui/*.zi) $(ZI2C_BIN)
	mkdir -p $(BUILD_DIR)/native
	$(ZI2C_BIN) --root . --module-path ../src/ui $(ZIRAN_STD_PATH) \
		-o $(BUILD_DIR)/native hello_host.zi

$(BUILD_DIR)/hello-host: $(BUILD_DIR)/native/hello_host.c $(NATIVE_LIB) Makefile
	$(CC) -std=c11 -Wall -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/native $(BUILD_DIR)/native/hello_host.c \
		$(BUILD_DIR)/native/hello.c $(BUILD_DIR)/native/math.c \
		$(NATIVE_LIB) -lm -o $@

$(BUILD_DIR)/hello.svg: $(BUILD_DIR)/hello-host
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host > $@

test: all $(BUILD_DIR)/hello-saved.zib
	python3 -c 'import xml.etree.ElementTree as ET; root=ET.parse("$(BUILD_DIR)/hello.svg").getroot(); ns="{http://www.w3.org/2000/svg}"; assert root.tag==ns+"svg"; assert [node.text for node in root.iter(ns+"text")]==["Hello, Ziran"]*3+["Clicked!"]; assert len(list(root.iter(ns+"rect")))>=4'

$(BUILD_DIR)/desktop-hello-native/desktop_hello.c: desktop_hello.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi hello.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN) Makefile
	mkdir -p $(BUILD_DIR)/desktop-hello-native
	$(RM) $(BUILD_DIR)/desktop-hello-native/*.c $(BUILD_DIR)/desktop-hello-native/*.h
	$(ZI2C_BIN) --root . --module-path ../src/ui --module-path ../src/backend \
		--module-path $(ZIRAN_DIR)/std \
		--entry desktop_hello:main -o $(BUILD_DIR)/desktop-hello-native desktop_hello.zi

$(BUILD_DIR)/desktop-image-native/desktop_image.c: desktop_image.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi image_demo.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN) Makefile
	mkdir -p $(BUILD_DIR)/desktop-image-native
	$(RM) $(BUILD_DIR)/desktop-image-native/*.c $(BUILD_DIR)/desktop-image-native/*.h
	$(ZI2C_BIN) --root . --module-path ../src/ui --module-path ../src/backend \
		--module-path $(ZIRAN_DIR)/std \
		--entry desktop_image:main -o $(BUILD_DIR)/desktop-image-native desktop_image.zi

$(BUILD_DIR)/desktop-text-native/desktop_text.c: desktop_text.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi text_hello.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN) Makefile
	mkdir -p $(BUILD_DIR)/desktop-text-native
	$(RM) $(BUILD_DIR)/desktop-text-native/*.c $(BUILD_DIR)/desktop-text-native/*.h
	$(ZI2C_BIN) --root . --module-path ../src/ui --module-path ../src/backend \
		--module-path $(ZIRAN_DIR)/std \
		--entry desktop_text:main -o $(BUILD_DIR)/desktop-text-native desktop_text.zi

$(BUILD_DIR)/desktop-time-picker-native/desktop_time_picker.c: desktop_time_picker.zi time_picker_demo.zi ../src/backend/desktop_runtime.zi ../src/backend/cairo_raster.zi $(wildcard ../src/ui/*.zi ../src/ui/Widgets/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN) Makefile
	mkdir -p $(BUILD_DIR)/desktop-time-picker-native
	$(RM) $(BUILD_DIR)/desktop-time-picker-native/*.c $(BUILD_DIR)/desktop-time-picker-native/*.h
	$(ZI2C_BIN) --root . --module-path ../src/ui --module-path ../src/backend \
		--module-path $(ZIRAN_DIR)/std \
		--entry desktop_time_picker:main -o $(BUILD_DIR)/desktop-time-picker-native desktop_time_picker.zi

# The desktop host draws with Cairo and Pango and finds fonts with Fontconfig.
DESKTOP_LIBS = $(shell pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread

.PHONY: widget-catalog
widget-catalog: $(BUILD_DIR)/widget-catalog-desktop

$(BUILD_DIR)/desktop-widget-catalog-native/.generated: desktop_widget_catalog.zi widget_catalog.zi $(wildcard ../src/ui/*.zi ../src/ui/Widgets/*.zi ../src/ui/Kryon/*.zi ../src/backend/*.zi) $(ZI2C_BIN) Makefile
	mkdir -p $(BUILD_DIR)/desktop-widget-catalog-native
	$(ZI2C_BIN) --root . --module-path ../src/ui --module-path ../src/backend \
		$(ZIRAN_STD_PATH) --entry desktop_widget_catalog:main \
		-o $(BUILD_DIR)/desktop-widget-catalog-native desktop_widget_catalog.zi
	touch $@

$(BUILD_DIR)/widget-catalog-desktop: $(BUILD_DIR)/desktop-widget-catalog-native/.generated Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-widget-catalog-native \
		$(BUILD_DIR)/desktop-widget-catalog-native/*.c \
		$(DESKTOP_LIBS) -o $@

$(BUILD_DIR)/hello-desktop: $(BUILD_DIR)/desktop-hello-native/desktop_hello.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-hello-native \
		$(BUILD_DIR)/desktop-hello-native/*.c \
		$(DESKTOP_LIBS) -o $@

$(BUILD_DIR)/image-desktop: $(BUILD_DIR)/desktop-image-native/desktop_image.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-image-native \
		$(BUILD_DIR)/desktop-image-native/*.c \
		$(DESKTOP_LIBS) -o $@

$(BUILD_DIR)/text-desktop: $(BUILD_DIR)/desktop-text-native/desktop_text.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-text-native \
		$(BUILD_DIR)/desktop-text-native/*.c \
		$(DESKTOP_LIBS) -o $@

$(BUILD_DIR)/time-picker-desktop: $(BUILD_DIR)/desktop-time-picker-native/desktop_time_picker.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-time-picker-native \
		$(BUILD_DIR)/desktop-time-picker-native/*.c \
		$(DESKTOP_LIBS) -o $@

desktop: $(BUILD_DIR)/hello-desktop

desktop-test: desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/hello-desktop.png \
		xvfb-run -a $(BUILD_DIR)/hello-desktop

desktop-image-test: $(BUILD_DIR)/image-saved.zib $(BUILD_DIR)/image-desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/image-desktop.png \
		xvfb-run -a $(BUILD_DIR)/image-desktop

desktop-text-test: $(BUILD_DIR)/text-saved.zib $(BUILD_DIR)/text-desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/text-desktop.png \
		xvfb-run -a $(BUILD_DIR)/text-desktop

# The TimePicker example. `make -C examples time-picker` builds it; run
# ../build/examples/ziran/time-picker-desktop from examples/ to open it.
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
