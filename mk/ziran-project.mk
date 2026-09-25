# Shared build route for a Ziran application configured by kryon.toml.
ifndef PROJECT_NAME
$(error PROJECT_NAME is required; run this through kryon)
endif
ifndef PROJECT_ENTRY
$(error PROJECT_ENTRY is required; run this through kryon)
endif
ifndef KRYON_DIR
$(error KRYON_DIR is required; run this through kryon)
endif
ifndef ZIRAN_DIR
$(error ZIRAN_DIR is required; run this through kryon)
endif
ifndef PROJECT_PROFILE
$(error PROJECT_PROFILE is required; run this through kryon)
endif

ifneq ($(PROJECT_BACKEND)/$(PROJECT_CODEGEN),terminal/c99)
$(error backend $(PROJECT_BACKEND) with codegen $(PROJECT_CODEGEN) is not available)
endif

CC ?= cc
ZIRAN := $(ZIRAN_DIR)/build/bin/ziran
GEN_DIR := build/generated
IR_DIR := $(GEN_DIR)/ir
IR_STAMP := $(IR_DIR)/.complete
C_DIR := $(GEN_DIR)/c
C_STAMP := $(C_DIR)/.complete
PROGRAM := build/$(PROJECT_NAME)
HOST := $(KRYON_DIR)/src/backend/terminal_run.zi
APP_SOURCES := $(wildcard src/*.zi)
UI_SOURCES := $(wildcard $(KRYON_DIR)/src/ui/*.zi)
HOST_SOURCES := $(wildcard $(KRYON_DIR)/src/backend/terminal*.zi)

.PHONY: run build check toolchain

toolchain:
	$(MAKE) --no-print-directory -C $(ZIRAN_DIR) all

build: toolchain
	$(MAKE) --no-print-directory -f $(KRYON_DIR)/mk/ziran-project.mk $(PROGRAM) \
		PROJECT_NAME=$(PROJECT_NAME) PROJECT_ENTRY=$(PROJECT_ENTRY) \
		PROJECT_BACKEND=$(PROJECT_BACKEND) PROJECT_CODEGEN=$(PROJECT_CODEGEN) \
		PROJECT_PROFILE=$(PROJECT_PROFILE) KRYON_DIR=$(KRYON_DIR) ZIRAN_DIR=$(ZIRAN_DIR)

$(IR_STAMP): $(PROJECT_ENTRY) $(APP_SOURCES) $(UI_SOURCES) $(HOST_SOURCES) $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(IR_DIR)
	rm -f $(IR_DIR)/*.zir $(IR_STAMP)
	$(ZIRAN) ir --entry terminal_run:main --root $(KRYON_DIR)/src/backend \
		--module-path src --module-path $(KRYON_DIR)/src/ui \
		-o $(IR_DIR) $(HOST)
	test -f $(IR_DIR)/terminal_run.zir
	touch $(IR_STAMP)

$(C_STAMP): $(IR_STAMP) $(ZIRAN_DIR)/build/bin/zi2c $(ZIRAN) $(KRYON_DIR)/mk/ziran-project.mk
	mkdir -p $(C_DIR)
	rm -f $(C_DIR)/*.c $(C_DIR)/*.h $(C_STAMP) $(GEN_DIR)/*.c $(GEN_DIR)/*.h $(GEN_DIR)/.complete
	$(ZIRAN) build --target=c --entry terminal_run:main \
		--root $(IR_DIR) -o $(C_DIR) $(IR_DIR)/terminal_run.zir
	test -f $(C_DIR)/terminal_run.c
	touch $(C_STAMP)

$(GEN_DIR)/compile_commands.json: $(C_STAMP) $(KRYON_DIR)/tools/write-compile-commands.py
	python3 $(KRYON_DIR)/tools/write-compile-commands.py $(ZIRAN_DIR)/include '$(CC)'

$(PROGRAM): $(C_STAMP) $(GEN_DIR)/compile_commands.json
	@temporary=$$(mktemp build/$(PROJECT_NAME).XXXXXX); \
		trap 'rm -f "$$temporary"' EXIT; \
		$(CC) -std=c99 -pedantic-errors -O2 -ffunction-sections -fdata-sections \
		-I$(ZIRAN_DIR)/include -I$(C_DIR) $(C_DIR)/*.c \
		-Wl,--gc-sections -lm -o "$$temporary" && \
		chmod 755 "$$temporary" && mv "$$temporary" $@

run: build
	./$(PROGRAM)

check: toolchain
	$(ZIRAN) check --root $(KRYON_DIR)/src/backend \
		--module-path src --module-path $(KRYON_DIR)/src/ui $(HOST)
