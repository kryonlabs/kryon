.DEFAULT_GOAL := all

CC ?= cc
ZIRAN_DIR ?= ../../ziran
ZI2ZIR_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zir)
ZI2ZIB_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zib)
ZI2C_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2c)
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
BUILD_DIR ?= ../build/examples/ziran
NATIVE_LIB := ../build/ziran/libkryon.a

.PHONY: all test desktop desktop-test desktop-image-test clean
all: $(BUILD_DIR)/hello.svg $(BUILD_DIR)/hello.zib

$(ZI2ZIB_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(ZI2C_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(NATIVE_LIB): ../Makefile ../src/ui/modules.txt $(wildcard ../src/ui/*.zi)
	$(MAKE) -C .. all

$(BUILD_DIR)/hello.zib: hello.zi $(wildcard ../src/ui/*.zi) $(ZI2ZIB_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZI2ZIB_BIN) bundle --root . --module-path ../src/ui \
		--entry hello:Frame -o $@ hello.zi

$(BUILD_DIR)/hello-saved.zib: $(BUILD_DIR)/hello.zib
	$(ZI2ZIR_BIN) --root . --module-path ../src/ui \
		-o $(BUILD_DIR)/ir hello.zi
	$(ZI2ZIB_BIN) bundle --root $(BUILD_DIR)/ir \
		--module-path $(BUILD_DIR)/ir --entry hello:Frame \
		-o $@ $(BUILD_DIR)/ir/hello.zir
	cmp $(BUILD_DIR)/hello.zib $@

$(BUILD_DIR)/image.zib: image_demo.zi $(wildcard ../src/ui/*.zi) $(ZI2ZIB_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZI2ZIB_BIN) bundle --root . --module-path ../src/ui \
		--entry image_demo:Frame -o $@ image_demo.zi

$(BUILD_DIR)/image-saved.zib: $(BUILD_DIR)/image.zib
	$(ZI2ZIR_BIN) --root . --module-path ../src/ui \
		-o $(BUILD_DIR)/image-ir image_demo.zi
	$(ZI2ZIB_BIN) bundle --root $(BUILD_DIR)/image-ir \
		--module-path $(BUILD_DIR)/image-ir --entry image_demo:Frame \
		-o $@ $(BUILD_DIR)/image-ir/image_demo.zir
	cmp $(BUILD_DIR)/image.zib $@

$(BUILD_DIR)/native/hello_host.c: hello_host.zi hello.zi $(wildcard ../src/ui/*.zi) $(ZI2C_BIN)
	mkdir -p $(BUILD_DIR)/native
	$(ZI2C_BIN) --strict --root . --module-path ../src/ui \
		-o $(BUILD_DIR)/native hello_host.zi

$(BUILD_DIR)/hello-host: $(BUILD_DIR)/native/hello_host.c $(NATIVE_LIB) Makefile
	$(CC) -std=c11 -Wall -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/native $(BUILD_DIR)/native/hello_host.c \
		$(BUILD_DIR)/native/hello.c $(NATIVE_LIB) -lm -o $@

$(BUILD_DIR)/hello.svg: $(BUILD_DIR)/hello-host
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host > $@

test: all $(BUILD_DIR)/hello-saved.zib
	python3 -c 'import xml.etree.ElementTree as ET; root=ET.parse("$(BUILD_DIR)/hello.svg").getroot(); ns="{http://www.w3.org/2000/svg}"; assert root.tag==ns+"svg"; assert [node.text for node in root.iter(ns+"text")]==["Hello, Ziran"]*3+["Clicked!"]; assert len(list(root.iter(ns+"rect")))>=4'

$(BUILD_DIR)/desktop-hello-native/desktop_hello.c: desktop_hello.zi desktop_runtime.zi hello.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN)
	mkdir -p $(BUILD_DIR)/desktop-hello-native
	$(ZI2C_BIN) --strict --root . --module-path ../src/ui \
		--module-path $(ZIRAN_DIR)/std \
		-o $(BUILD_DIR)/desktop-hello-native desktop_hello.zi

$(BUILD_DIR)/desktop-image-native/desktop_image.c: desktop_image.zi desktop_runtime.zi image_demo.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_DIR)/std/c_string.zi $(ZI2C_BIN)
	mkdir -p $(BUILD_DIR)/desktop-image-native
	$(ZI2C_BIN) --strict --root . --module-path ../src/ui \
		--module-path $(ZIRAN_DIR)/std \
		-o $(BUILD_DIR)/desktop-image-native desktop_image.zi

$(BUILD_DIR)/hello-desktop: $(BUILD_DIR)/desktop-hello-native/desktop_hello.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-hello-native \
		$(BUILD_DIR)/desktop-hello-native/*.c \
		$$(pkg-config --libs sdl2 cairo) -lm -o $@

$(BUILD_DIR)/image-desktop: $(BUILD_DIR)/desktop-image-native/desktop_image.c Makefile
	$(CC) -std=c11 -O2 -I$(ZIRAN_INCLUDE) \
		-I$(BUILD_DIR)/desktop-image-native \
		$(BUILD_DIR)/desktop-image-native/*.c \
		$$(pkg-config --libs sdl2 cairo) -lm -o $@

desktop: $(BUILD_DIR)/hello-desktop

desktop-test: desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/hello-desktop.png \
		xvfb-run -a $(BUILD_DIR)/hello-desktop

desktop-image-test: $(BUILD_DIR)/image-saved.zib $(BUILD_DIR)/image-desktop
	env -u DISPLAY -u WAYLAND_DISPLAY \
		KRYON_CAPTURE_PATH=$(BUILD_DIR)/image-desktop.png \
		xvfb-run -a $(BUILD_DIR)/image-desktop

clean:
	$(RM) -r $(BUILD_DIR)
