.DEFAULT_GOAL := all

CC ?= cc
ZIRAN_DIR ?= ../../ziran
ZI2ZIR_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zir)
ZI2ZIB_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/zi2zib)
ZIRAN_LIB ?= $(abspath $(ZIRAN_DIR)/build/libziran.a)
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)
BUILD_DIR ?= ../build/examples/ziran
HOST_LIB := ../build/ziran/libkryon_host.a

.PHONY: all test desktop desktop-test clean
all: $(BUILD_DIR)/hello.svg

$(ZI2ZIB_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(HOST_LIB):
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

$(BUILD_DIR)/hello-host: hello_host.c $(HOST_LIB) $(ZIRAN_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -I../build/ziran/c \
		-I$(ZIRAN_INCLUDE) hello_host.c $(HOST_LIB) \
		$(ZIRAN_LIB) -o $@

$(BUILD_DIR)/hello.svg: $(BUILD_DIR)/hello-host $(BUILD_DIR)/hello.zib
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host $(BUILD_DIR)/hello.zib $@

$(BUILD_DIR)/hello-saved.svg: $(BUILD_DIR)/hello-host $(BUILD_DIR)/hello-saved.zib
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host $(BUILD_DIR)/hello-saved.zib $@

test: all $(BUILD_DIR)/hello-saved.svg
	cmp $(BUILD_DIR)/hello.svg $(BUILD_DIR)/hello-saved.svg
	python3 -c 'import xml.etree.ElementTree as ET; root=ET.parse("$(BUILD_DIR)/hello.svg").getroot(); ns="{http://www.w3.org/2000/svg}"; assert root.tag==ns+"svg"; assert [node.text for node in root.iter(ns+"text")]==["Hello, Ziran"]*3+["Clicked!"]; assert len(list(root.iter(ns+"rect")))>=4'

$(BUILD_DIR)/hello-desktop: desktop_host.c $(HOST_LIB) $(ZIRAN_LIB)
	$(CC) -std=c11 -Wall -Wextra -I../build/ziran/c \
		-I$(ZIRAN_INCLUDE) $$(pkg-config --cflags sdl2 cairo) \
		desktop_host.c $(HOST_LIB) $(ZIRAN_LIB) \
		$$(pkg-config --libs sdl2 cairo) -lm -o $@

desktop: $(BUILD_DIR)/hello.zib $(BUILD_DIR)/hello-desktop

desktop-test: desktop
	env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a \
		$(BUILD_DIR)/hello-desktop $(BUILD_DIR)/hello.zib \
		--self-test $(BUILD_DIR)/hello-desktop.png

clean:
	$(RM) -r $(BUILD_DIR)
