.DEFAULT_GOAL := all

CC ?= cc
ZIRAN_DIR ?= ../../ziran
ZIRAN_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/ziran)
ZIRAN_LIB ?= $(abspath $(ZIRAN_DIR)/build/libziran.a)
BUILD_DIR ?= ../build/examples/ziran
HOST_LIB := ../build/ziran/libkryon_host.a

.PHONY: all test clean
all: $(BUILD_DIR)/hello.svg

$(ZIRAN_BIN):
	$(MAKE) -C $(ZIRAN_DIR) all

$(HOST_LIB):
	$(MAKE) -C .. all

$(BUILD_DIR)/hello.zib: hello.zi $(wildcard ../src/ui/*.zi) $(ZIRAN_BIN)
	mkdir -p $(BUILD_DIR)
	$(ZIRAN_BIN) bundle --root . --module-path ../src/ui \
		--entry hello:Frame -o $@ hello.zi

$(BUILD_DIR)/hello-saved.zib: $(BUILD_DIR)/hello.zib
	$(ZIRAN_BIN) ir --root . --module-path ../src/ui \
		-o $(BUILD_DIR)/ir hello.zi
	$(ZIRAN_BIN) bundle --root $(BUILD_DIR)/ir \
		--module-path $(BUILD_DIR)/ir --entry hello:Frame \
		-o $@ $(BUILD_DIR)/ir/hello.zir
	cmp $(BUILD_DIR)/hello.zib $@

$(BUILD_DIR)/hello-host: hello_host.c ../include/kryon_portable_host.h $(HOST_LIB)
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -Wall -Wextra -I../include \
		-I$(ZIRAN_DIR)/include hello_host.c $(HOST_LIB) \
		$(ZIRAN_LIB) -o $@

$(BUILD_DIR)/hello.svg: $(BUILD_DIR)/hello-host $(BUILD_DIR)/hello.zib
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host $(BUILD_DIR)/hello.zib $@

$(BUILD_DIR)/hello-saved.svg: $(BUILD_DIR)/hello-host $(BUILD_DIR)/hello-saved.zib
	env -u DISPLAY -u WAYLAND_DISPLAY \
		$(BUILD_DIR)/hello-host $(BUILD_DIR)/hello-saved.zib $@

test: all $(BUILD_DIR)/hello-saved.svg
	cmp $(BUILD_DIR)/hello.svg $(BUILD_DIR)/hello-saved.svg
	python3 -c 'import xml.etree.ElementTree as ET; root=ET.parse("$(BUILD_DIR)/hello.svg").getroot(); ns="{http://www.w3.org/2000/svg}"; assert root.tag==ns+"svg"; assert any(node.text=="Hello, Ziran" for node in root.iter(ns+"text")); assert len(list(root.iter(ns+"rect")))>=2'

clean:
	$(RM) -r $(BUILD_DIR)
