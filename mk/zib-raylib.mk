# Generic native player for the same Frame ZIB used by desktop and web.
# Cross builds supply a target CC/AR, matching raylib archive and system libs.
ZIB_RAYLIB_DIR ?= $(KRYON_DIR)/build/zib-raylib
ZIB_RAYLIB_PROGRAM ?= $(ZIB_RAYLIB_DIR)/zib
ZIB_RAYLIB_CC ?= $(CC)
ZIB_RAYLIB_AR ?= ar
ZIB_RAYLIB_OBJCOPY ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),true,objcopy)
ZIB_RAYLIB_PRIVATE_HEADERS ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),$(ZIB_RAYLIB_DIR)/runtime/private)
ZIB_RAYLIB_LDFLAGS ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),-Wl$(comma)-dead_strip,-Wl$(comma)--gc-sections)
comma := ,
ZIB_RAYLIB_CFLAGS ?= -O2
ZIB_RAYLIB_DEFINES ?= $(RAYLIB_HOST_DEFINES)
ZIB_RAYLIB_ARCHIVE ?= $(KRYON_DIR)/build/raylib-ziran/libraylib.a
ZIB_RAYLIB_LIBS ?= $(RAYLIB_PLATFORM_LIBS) -lm
ZIB_RAYLIB_GENERATED := $(ZIB_RAYLIB_DIR)/generated
ZIB_RAYLIB_RUNTIME := $(ZIB_RAYLIB_DIR)/runtime/libziran.a
ZIB_RAYLIB_FONT := $(dir $(ZIB_RAYLIB_PROGRAM))kryon-font.ttf
ZIB_RAYLIB_DEFINE_ARGS := --define BUNDLE_NATIVE_RENDERER $(foreach define,$(ZIB_RAYLIB_DEFINES),--define $(define))
ZIB_RAYLIB_SOURCES := $(KRYON_DIR)/tools/zib_raylib.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi $(KRYON_DIR)/src/ui/*.zi $(ZIRAN_DIR)/std/*.zi)
ZIB_RAYLIB_COMPILER := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.[ch] $(ZIRAN_DIR)/cmd/*.zi $(ZIRAN_DIR)/include/*.h $(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c)
ifneq ($(strip $(ZIB_RAYLIB_PRIVATE_HEADERS)),)
ZIB_RAYLIB_PRIVATE_STAMP := $(ZIB_RAYLIB_PRIVATE_HEADERS)/.complete
$(ZIB_RAYLIB_PRIVATE_STAMP): $(wildcard $(ZIRAN_DIR)/cmd/zir/zir_*_internal.h) $(ZIRAN_DIR)/scripts/private-symbols.py $(ZIB_RAYLIB_DIR)/config
	mkdir -p $(ZIB_RAYLIB_PRIVATE_HEADERS)
	@for group in parse check emit vm; do \
		python3 $(ZIRAN_DIR)/scripts/private-symbols.py $(ZIRAN_DIR)/cmd/zir/zir_$${group}_internal.h $(ZIB_RAYLIB_PRIVATE_HEADERS)/$${group}.txt && \
		sed "s/.*/#define & private_$${group}_&/" $(ZIB_RAYLIB_PRIVATE_HEADERS)/$${group}.txt > $(ZIB_RAYLIB_PRIVATE_HEADERS)/$${group}.h || exit 1; \
	done
	touch $@
endif

.PHONY: zib-raylib zib-raylib-config
zib-raylib: $(ZIB_RAYLIB_PROGRAM)

$(ZIB_RAYLIB_DIR)/config: zib-raylib-config
	@mkdir -p $(ZIB_RAYLIB_DIR)
	@printf '%s\n' "$(ZIRAN_DIR)" "$(ZIB_RAYLIB_CC)" "$(ZIB_RAYLIB_AR)" "$(ZIB_RAYLIB_OBJCOPY)" "$(ZIB_RAYLIB_PRIVATE_HEADERS)" "$(ZIB_RAYLIB_CFLAGS)" "$(ZIB_RAYLIB_DEFINES)" "$(ZIB_RAYLIB_ARCHIVE)" "$(ZIB_RAYLIB_LDFLAGS)" "$(ZIB_RAYLIB_LIBS)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZIB_RAYLIB_GENERATED)/.complete: $(ZIB_RAYLIB_SOURCES) $(ZIB_RAYLIB_COMPILER) $(KRYON_DIR)/mk/zib-raylib.mk $(ZIB_RAYLIB_DIR)/config
	mkdir -p $(ZIB_RAYLIB_GENERATED)/ir $(ZIB_RAYLIB_GENERATED)/c
	rm -f $(ZIB_RAYLIB_GENERATED)/ir/*.zir $(ZIB_RAYLIB_GENERATED)/c/*.[ch]
	$(ZIRAN_DIR)/build/bin/ziran ir $(ZIB_RAYLIB_DEFINE_ARGS) --root $(KRYON_DIR)/tools \
		--module-path $(KRYON_DIR)/src/backend --module-path $(KRYON_DIR)/src/ui \
		--module-path $(ZIRAN_DIR)/std -o $(ZIB_RAYLIB_GENERATED)/ir $(KRYON_DIR)/tools/zib_raylib.zi
	$(ZIRAN_DIR)/build/bin/zi2c $(ZIB_RAYLIB_DEFINE_ARGS) --root $(ZIB_RAYLIB_GENERATED)/ir \
		-o $(ZIB_RAYLIB_GENERATED)/c $(ZIB_RAYLIB_GENERATED)/ir/*.zir
	touch $@

$(ZIB_RAYLIB_RUNTIME): $(wildcard $(ZIRAN_DIR)/cmd/zir/*.[ch] $(ZIRAN_DIR)/include/*.h) $(ZIB_RAYLIB_DIR)/config $(KRYON_DIR)/mk/zib-raylib.mk $(ZIB_RAYLIB_PRIVATE_STAMP)
	$(MAKE) -B -j4 -C $(ZIRAN_DIR) BOOTSTRAP=1 BUILD_DIR=$(ZIB_RAYLIB_DIR)/runtime \
		CC="$(ZIB_RAYLIB_CC)" AR="$(ZIB_RAYLIB_AR)" OBJCOPY="$(ZIB_RAYLIB_OBJCOPY)" \
		WASM_PRIVATE_HEADERS="$(ZIB_RAYLIB_PRIVATE_HEADERS)" CFLAGS="$(ZIB_RAYLIB_CFLAGS)" $@

$(ZIB_RAYLIB_FONT): $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
	@mkdir -p $(dir $@)
	cp $< $@

$(ZIB_RAYLIB_PROGRAM): $(ZIB_RAYLIB_GENERATED)/.complete $(ZIB_RAYLIB_RUNTIME) $(ZIB_RAYLIB_ARCHIVE) $(ZIB_RAYLIB_FONT)
	$(ZIB_RAYLIB_CC) $(ZIB_RAYLIB_CFLAGS) -ffunction-sections -fdata-sections -Wno-main \
		-I$(ZIRAN_DIR)/include -iquote $(ZIB_RAYLIB_GENERATED)/c \
		$(ZIB_RAYLIB_GENERATED)/c/*.c $(ZIB_RAYLIB_RUNTIME) $(ZIB_RAYLIB_ARCHIVE) \
		$(ZIB_RAYLIB_LDFLAGS) $(ZIB_RAYLIB_LIBS) -o $@
