.DEFAULT_GOAL := all

CC ?= cc
CXX ?= c++
AR ?= ar
BUILD_DIR ?= build/ziran
ZIRAN_DIR ?= ../ziran
ZIRAN_BIN ?= $(abspath $(ZIRAN_DIR)/build/bin/ziran)
ZIRAN_INCLUDE ?= $(abspath $(ZIRAN_DIR)/include)

SOURCE := $(addprefix src/ui/,$(shell cat src/ui/modules.txt))
MODULES := $(basename $(notdir $(SOURCE)))
OBJECTS := $(addprefix $(BUILD_DIR)/obj/,$(addsuffix .o,$(MODULES)))

.PHONY: all check test ziran-test clean
all: $(BUILD_DIR)/libkryon.a

# Kryon is an ordinary Ziran library. Platform hosts are linked separately.
$(BUILD_DIR)/libkryon.a: $(SOURCE) src/ui/modules.txt
	$(MAKE) -C $(ZIRAN_DIR) all
	mkdir -p $(BUILD_DIR)/ir $(BUILD_DIR)/c $(BUILD_DIR)/cpp $(BUILD_DIR)/go $(BUILD_DIR)/obj $(BUILD_DIR)/obj-cpp
	$(ZIRAN_BIN) ir --root src/ui -o $(BUILD_DIR)/ir $(SOURCE)
	$(ZIRAN_BIN) build --target=c --strict --root src/ui -o $(BUILD_DIR)/c $(SOURCE)
	$(ZIRAN_BIN) build --target=cpp --strict --root src/ui -o $(BUILD_DIR)/cpp $(SOURCE)
	$(ZIRAN_BIN) build --target=go --strict --root src/ui -o $(BUILD_DIR)/go $(SOURCE)
	@for module in $(MODULES); do \
		$(CC) -std=c11 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/c -c $(BUILD_DIR)/c/$$module.c -o $(BUILD_DIR)/obj/$$module.o || exit 1; \
		$(CXX) -std=c++17 -I$(ZIRAN_INCLUDE) -I$(BUILD_DIR)/cpp -c $(BUILD_DIR)/cpp/$$module.cpp -o $(BUILD_DIR)/obj-cpp/$$module.o || exit 1; \
	done
	rm -f $@
	$(AR) rcs $@ $(OBJECTS)
	GO111MODULE=off go test ./$(BUILD_DIR)/go

ziran-test:
	@for test_file in tests/ziran_*_test.sh; do sh "$$test_file" || exit 1; done

check: all ziran-test
test: check

clean:
	rm -rf $(BUILD_DIR)
