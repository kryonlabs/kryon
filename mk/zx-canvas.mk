# The browser shell contains the ZX interpreter and Canvas renderer, not an
# application's generated native UI. ZX_CANVAS_BUNDLE is the complete app.
ZX_CANVAS_DIR ?= $(KRYON_DIR)/build/zx-canvas
ZX_CANVAS_PROGRAM ?= $(ZX_CANVAS_DIR)/player.html
ZX_CANVAS_CC ?= $(or $(EMCC),$(shell command -v emcc 2>/dev/null),$(HOME)/emsdk/upstream/emscripten/emcc)
ZX_CANVAS_AR ?= $(or $(EMAR),$(wildcard $(dir $(ZX_CANVAS_CC))emar),$(shell command -v emar 2>/dev/null),$(HOME)/emsdk/upstream/emscripten/emar)
ZX_CANVAS_CACHE ?= $(KRYON_DIR)/build/emscripten-cache/zx-canvas
# A project page names its app in a generated copy of this shell.
ZX_CANVAS_SHELL ?= $(KRYON_DIR)/mk/zx-shell.html
ZX_CANVAS_GENERATED := $(ZX_CANVAS_DIR)/generated
ZX_CANVAS_RUNTIME := $(ZX_CANVAS_DIR)/runtime/libziran.a
ZX_CANVAS_SOURCES := $(KRYON_DIR)/tools/zx_canvas.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi $(KRYON_DIR)/src/ui/*.zi $(ZIRAN_DIR)/std/*.zi)
ZX_CANVAS_COMPILER := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.[ch] $(ZIRAN_DIR)/cmd/*.zi $(ZIRAN_DIR)/include/*.h $(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c)
ZX_CANVAS_RUNTIME_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir/*.[ch] $(ZIRAN_DIR)/include/*.h)

.PHONY: zx-canvas zx-canvas-config
zx-canvas: $(ZX_CANVAS_PROGRAM)

$(ZX_CANVAS_DIR)/selected-bundle: zx-canvas-config
	@mkdir -p $(ZX_CANVAS_DIR)
	@printf '%s\n' "$(ZX_CANVAS_BUNDLE)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZX_CANVAS_DIR)/toolchain: zx-canvas-config
	@mkdir -p $(ZX_CANVAS_DIR)
	@printf '%s\n' "$(ZIRAN_DIR)" "$(ZX_CANVAS_CC)" "$(ZX_CANVAS_AR)" "$(ZX_CANVAS_CACHE)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZX_CANVAS_GENERATED)/.complete: $(ZX_CANVAS_SOURCES) $(ZX_CANVAS_COMPILER) $(KRYON_DIR)/mk/zx-canvas.mk $(ZX_CANVAS_DIR)/toolchain
	mkdir -p $(ZX_CANVAS_GENERATED)/ir $(ZX_CANVAS_GENERATED)/c
	rm -f $(ZX_CANVAS_GENERATED)/ir/*.zir $(ZX_CANVAS_GENERATED)/c/*.[ch]
	$(ZIRAN_DIR)/build/bin/ziran ir --define PLATFORM_WEB --define BUNDLE_NATIVE_RENDERER --root $(KRYON_DIR)/tools \
		--module-path $(KRYON_DIR)/src/backend --module-path $(KRYON_DIR)/src/ui \
		--module-path $(ZIRAN_DIR)/std -o $(ZX_CANVAS_GENERATED)/ir \
		$(KRYON_DIR)/tools/zx_canvas.zi $(KRYON_DIR)/src/backend/canvas_raster.zi
	$(ZIRAN_DIR)/build/bin/zi2c --define PLATFORM_WEB --define BUNDLE_NATIVE_RENDERER --root $(ZX_CANVAS_GENERATED)/ir \
		-o $(ZX_CANVAS_GENERATED)/c $(ZX_CANVAS_GENERATED)/ir/*.zir
	touch $@

$(ZX_CANVAS_RUNTIME): $(ZX_CANVAS_RUNTIME_SOURCES) $(ZX_CANVAS_DIR)/toolchain
	EM_CACHE=$(ZX_CANVAS_CACHE) $(MAKE) -B -C $(ZIRAN_DIR) BOOTSTRAP=1 \
		BUILD_DIR=$(ZX_CANVAS_DIR)/runtime CC=$(ZX_CANVAS_CC) AR=$(ZX_CANVAS_AR) \
		OBJCOPY=true CFLAGS=-O2 $(ZX_CANVAS_RUNTIME)

$(ZX_CANVAS_PROGRAM): $(ZX_CANVAS_GENERATED)/.complete $(ZX_CANVAS_RUNTIME) $(ZX_CANVAS_BUNDLE) $(ZX_CANVAS_DIR)/selected-bundle $(ZX_CANVAS_SHELL)
	@mkdir -p $(dir $(ZX_CANVAS_PROGRAM))
	EM_CACHE=$(ZX_CANVAS_CACHE) $(ZX_CANVAS_CC) -O2 -I$(ZIRAN_DIR)/include \
		-iquote $(ZX_CANVAS_GENERATED)/c $(ZX_CANVAS_GENERATED)/c/*.c $(ZX_CANVAS_RUNTIME) \
		--js-library $(ZIRAN_DIR)/web/ziran_web.js -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS \
		--embed-file $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf \
		$(if $(ZX_CANVAS_BUNDLE),--embed-file "$(ZX_CANVAS_BUNDLE)@/app.zx") \
		-sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 -sASYNCIFY_STACK_SIZE=1048576 \
		-sEXIT_RUNTIME=1 -sENVIRONMENT=web --shell-file $(ZX_CANVAS_SHELL) \
		-o $(ZX_CANVAS_PROGRAM)
