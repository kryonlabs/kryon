# Generic native player for the same Frame ZX used by desktop and web.
# Cross builds supply a target CC/AR, matching raylib archive and system libs.
ZX_RAYLIB_DIR ?= $(KRYON_DIR)/build/zx-raylib
ZX_RAYLIB_PROGRAM ?= $(ZX_RAYLIB_DIR)/zx
ZX_RAYLIB_CC ?= $(CC)
ZX_RAYLIB_AR ?= ar
ZX_RAYLIB_OBJCOPY ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),true,objcopy)
ZX_RAYLIB_PRIVATE_HEADERS ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),$(ZX_RAYLIB_DIR)/runtime/private)
ZX_RAYLIB_LDFLAGS ?= $(if $(filter Darwin,$(RAYLIB_HOST_OS)),-Wl$(comma)-dead_strip,-Wl$(comma)--gc-sections)
comma := ,
ZX_RAYLIB_CFLAGS ?= -O2
ZX_RAYLIB_DEFINES ?= $(RAYLIB_HOST_DEFINES)
ZX_RAYLIB_ARCHIVE ?= $(KRYON_DIR)/build/raylib-ziran/libraylib.a
ZX_RAYLIB_HTTP_LIBS ?= $(or $(shell pkg-config --libs libcurl 2>/dev/null),$(if $(filter Linux,$(RAYLIB_HOST_OS)),-l:libcurl.so.4,-lcurl))
ZX_RAYLIB_LIBS ?= $(RAYLIB_PLATFORM_LIBS) $(ZX_RAYLIB_HTTP_LIBS) -lm
ZX_RAYLIB_GENERATED := $(ZX_RAYLIB_DIR)/generated
ZX_RAYLIB_RUNTIME := $(ZX_RAYLIB_DIR)/runtime/libziran.a
ZX_RAYLIB_FONT := $(dir $(ZX_RAYLIB_PROGRAM))kryon-font.ttf
ZX_RAYLIB_DEFINE_ARGS := --define BUNDLE_NATIVE_RENDERER $(foreach define,$(ZX_RAYLIB_DEFINES),--define $(define))
ZX_RAYLIB_SOURCES := $(KRYON_DIR)/tools/zx_raylib.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi $(KRYON_DIR)/src/ui/*.zi $(ZIRAN_DIR)/std/*.zi)
ZX_RAYLIB_COMPILER := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.[ch] $(ZIRAN_DIR)/cmd/*.zi $(ZIRAN_DIR)/include/*.h $(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c)
ifneq ($(strip $(ZX_RAYLIB_PRIVATE_HEADERS)),)
ZX_RAYLIB_PRIVATE_STAMP := $(ZX_RAYLIB_PRIVATE_HEADERS)/.complete
$(ZX_RAYLIB_PRIVATE_STAMP): $(wildcard $(ZIRAN_DIR)/cmd/zir/zir_*_internal.h) $(ZIRAN_DIR)/scripts/private-symbols.py $(ZX_RAYLIB_DIR)/config
	mkdir -p $(ZX_RAYLIB_PRIVATE_HEADERS)
	@for group in parse check emit vm; do \
		python3 $(ZIRAN_DIR)/scripts/private-symbols.py $(ZIRAN_DIR)/cmd/zir/zir_$${group}_internal.h $(ZX_RAYLIB_PRIVATE_HEADERS)/$${group}.txt && \
		sed "s/.*/#define & private_$${group}_&/" $(ZX_RAYLIB_PRIVATE_HEADERS)/$${group}.txt > $(ZX_RAYLIB_PRIVATE_HEADERS)/$${group}.h || exit 1; \
	done
	touch $@
endif

.PHONY: zx-raylib zx-raylib-config
zx-raylib: $(ZX_RAYLIB_PROGRAM)

$(ZX_RAYLIB_DIR)/config: zx-raylib-config
	@mkdir -p $(ZX_RAYLIB_DIR)
	@printf '%s\n' "$(ZIRAN_DIR)" "$(ZX_RAYLIB_CC)" "$(ZX_RAYLIB_AR)" "$(ZX_RAYLIB_OBJCOPY)" "$(ZX_RAYLIB_PRIVATE_HEADERS)" "$(ZX_RAYLIB_CFLAGS)" "$(ZX_RAYLIB_DEFINES)" "$(ZX_RAYLIB_ARCHIVE)" "$(ZX_RAYLIB_LDFLAGS)" "$(ZX_RAYLIB_LIBS)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZX_RAYLIB_GENERATED)/.complete: $(ZX_RAYLIB_SOURCES) $(ZX_RAYLIB_COMPILER) $(KRYON_DIR)/mk/zx-raylib.mk $(ZX_RAYLIB_DIR)/config
	mkdir -p $(ZX_RAYLIB_GENERATED)/ir $(ZX_RAYLIB_GENERATED)/c
	rm -f $(ZX_RAYLIB_GENERATED)/ir/*.zir $(ZX_RAYLIB_GENERATED)/c/*.[ch]
	$(ZIRAN_DIR)/build/bin/ziran ir $(ZX_RAYLIB_DEFINE_ARGS) --root $(KRYON_DIR)/tools \
		--module-path $(KRYON_DIR)/src/backend --module-path $(KRYON_DIR)/src/ui \
		--module-path $(ZIRAN_DIR)/std -o $(ZX_RAYLIB_GENERATED)/ir $(KRYON_DIR)/tools/zx_raylib.zi
	$(ZIRAN_DIR)/build/bin/zi2c $(ZX_RAYLIB_DEFINE_ARGS) --root $(ZX_RAYLIB_GENERATED)/ir \
		-o $(ZX_RAYLIB_GENERATED)/c $(ZX_RAYLIB_GENERATED)/ir/*.zir
	touch $@

$(ZX_RAYLIB_RUNTIME): $(wildcard $(ZIRAN_DIR)/cmd/zir/*.[ch] $(ZIRAN_DIR)/include/*.h) $(ZX_RAYLIB_DIR)/config $(KRYON_DIR)/mk/zx-raylib.mk $(ZX_RAYLIB_PRIVATE_STAMP)
	$(MAKE) -B -j4 -C $(ZIRAN_DIR) BOOTSTRAP=1 BUILD_DIR=$(ZX_RAYLIB_DIR)/runtime \
		CC="$(ZX_RAYLIB_CC)" AR="$(ZX_RAYLIB_AR)" OBJCOPY="$(ZX_RAYLIB_OBJCOPY)" \
		WASM_PRIVATE_HEADERS="$(ZX_RAYLIB_PRIVATE_HEADERS)" CFLAGS="$(ZX_RAYLIB_CFLAGS)" $@

$(ZX_RAYLIB_FONT): $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf
	@mkdir -p $(dir $@)
	cp $< $@

$(ZX_RAYLIB_PROGRAM): $(ZX_RAYLIB_GENERATED)/.complete $(ZX_RAYLIB_RUNTIME) $(ZX_RAYLIB_ARCHIVE) $(ZX_RAYLIB_FONT)
	$(ZX_RAYLIB_CC) $(ZX_RAYLIB_CFLAGS) -ffunction-sections -fdata-sections -Wno-main \
		-I$(ZIRAN_DIR)/include -iquote $(ZX_RAYLIB_GENERATED)/c \
		$(ZX_RAYLIB_GENERATED)/c/*.c $(ZX_RAYLIB_RUNTIME) $(ZX_RAYLIB_ARCHIVE) \
		$(ZX_RAYLIB_LDFLAGS) $(ZX_RAYLIB_LIBS) -o $@
