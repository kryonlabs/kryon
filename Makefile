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

.PHONY: all check test ziran-test header-check clean
all: $(BUILD_DIR)/libkryon.a $(BUILD_DIR)/libkryon_host.a

$(BUILD_DIR)/ziran-toolchain.stamp: $(ZIRAN_SOURCES)
	$(MAKE) -C $(ZIRAN_DIR) BUILD_DIR=$(ZIRAN_BUILD_DIR) all
	mkdir -p $(BUILD_DIR)
	touch $@

$(BUILD_DIR)/c/image_canvas_types.h: src/backend/image_canvas_types.zi $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/c
	$(ZI2C_BIN) --no-main --strict --root src/backend \
		-o $(BUILD_DIR)/c src/backend/image_canvas_types.zi

$(BUILD_DIR)/c/kryon_portable_host.h: src/backend/kryon_portable_host.zi $(BUILD_DIR)/c/image_canvas_types.h $(ZIRAN_SOURCES)
	mkdir -p $(BUILD_DIR)/c
	$(ZI2C_BIN) --no-main --strict --root src/backend \
		-o $(BUILD_DIR)/c src/backend/kryon_portable_host.zi

$(BUILD_DIR)/libkryon_host.a: src/backend/frame_pacing_host.c src/backend/cursor_host.c src/backend/raster_host.c src/backend/font_metrics_host.c src/backend/image_host.c src/backend/image_software.c src/backend/kss_string_host.c src/backend/composition_host.c $(BUILD_DIR)/c/kryon_portable_host.h $(ZIRAN_INCLUDE)/ziran_host.h Makefile
	mkdir -p $(BUILD_DIR)
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/frame_pacing_host.c -o $(BUILD_DIR)/frame_pacing_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/cursor_host.c -o $(BUILD_DIR)/cursor_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/raster_host.c -o $(BUILD_DIR)/raster_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/font_metrics_host.c -o $(BUILD_DIR)/font_metrics_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/image_host.c -o $(BUILD_DIR)/image_host.o
	$(CC) -std=c11 -Iinclude -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c src/backend/image_software.c -o $(BUILD_DIR)/image_software.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/kss_string_host.c -o $(BUILD_DIR)/kss_string_host.o
	$(CC) -std=c11 -I$(BUILD_DIR)/c -I$(ZIRAN_INCLUDE) -c src/backend/composition_host.c -o $(BUILD_DIR)/composition_host.o
	rm -f $@
	$(AR) rcs $@ $(BUILD_DIR)/frame_pacing_host.o $(BUILD_DIR)/cursor_host.o $(BUILD_DIR)/raster_host.o $(BUILD_DIR)/font_metrics_host.o $(BUILD_DIR)/image_host.o $(BUILD_DIR)/image_software.o $(BUILD_DIR)/kss_string_host.o $(BUILD_DIR)/composition_host.o

# Kryon is an ordinary Ziran library. Platform hosts are linked separately.
$(BUILD_DIR)/libkryon.a: $(SOURCE) src/ui/modules.txt Makefile $(BUILD_DIR)/ziran-toolchain.stamp
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/cpp $(BUILD_DIR)/go $(BUILD_DIR)/obj $(BUILD_DIR)/obj-cpp
	$(ZI2ZIR_BIN) --root src/ui -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZI2C_BIN) --no-main --strict --root src/ui -o $(BUILD_DIR)/c $(SOURCE)
	$(ZI2CPP_BIN) --no-main --strict --root src/ui -o $(BUILD_DIR)/cpp $(SOURCE)
	$(ZI2GO_BIN) --no-main --strict --root src/ui -o $(BUILD_DIR)/go $(SOURCE)
	@for module in $(MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c $(BUILD_DIR)/c/$$module.c -o $(BUILD_DIR)/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/cpp -c $(BUILD_DIR)/cpp/$$module.cpp -o $(BUILD_DIR)/obj-cpp/$$module.o || exit 1; \
	done
	cd $(BUILD_DIR)/go && GO111MODULE=off go test .
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)

ziran-test: $(BUILD_DIR)/libkryon_host.a
	@for test_file in tests/ziran_*_test.sh; do \
		echo "$$test_file"; \
		sh "$$test_file" || exit 1; \
	done

header-check:
	sh tools/check-zi-header-free.sh

check: all ziran-test header-check
test: check

clean:
	rm -rf $(BUILD_DIR)
