# The browser shell contains the ZIB interpreter and Canvas renderer, not an
# application's generated native UI. ZIB_CANVAS_BUNDLE is the complete app.
ZIB_CANVAS_DIR ?= $(KRYON_DIR)/build/zib-canvas
ZIB_CANVAS_PROGRAM ?= $(ZIB_CANVAS_DIR)/player.html
ZIB_CANVAS_CC ?= $(or $(EMCC),$(shell command -v emcc 2>/dev/null),$(HOME)/emsdk/upstream/emscripten/emcc)
ZIB_CANVAS_AR ?= $(or $(EMAR),$(wildcard $(dir $(ZIB_CANVAS_CC))emar),$(shell command -v emar 2>/dev/null),$(HOME)/emsdk/upstream/emscripten/emar)
ZIB_CANVAS_CACHE ?= $(KRYON_DIR)/build/emscripten-cache/zib-canvas
ZIB_CANVAS_GENERATED := $(ZIB_CANVAS_DIR)/generated
ZIB_CANVAS_RUNTIME := $(ZIB_CANVAS_DIR)/runtime/libziran.a
ZIB_CANVAS_SOURCES := $(KRYON_DIR)/tools/zib_canvas.zi $(wildcard $(KRYON_DIR)/src/backend/*.zi $(KRYON_DIR)/src/ui/*.zi $(ZIRAN_DIR)/std/*.zi)
ZIB_CANVAS_COMPILER := $(wildcard $(ZIRAN_DIR)/cmd/zir*/*.[ch] $(ZIRAN_DIR)/cmd/*.zi $(ZIRAN_DIR)/include/*.h $(ZIRAN_DIR)/build/bin/ziran $(ZIRAN_DIR)/build/bin/zi2zir $(ZIRAN_DIR)/build/bin/zi2c)
ZIB_CANVAS_RUNTIME_SOURCES := $(wildcard $(ZIRAN_DIR)/cmd/zir/*.[ch] $(ZIRAN_DIR)/include/*.h)

.PHONY: zib-canvas zib-canvas-config
zib-canvas: $(ZIB_CANVAS_PROGRAM)

$(ZIB_CANVAS_DIR)/selected-bundle: zib-canvas-config
	@mkdir -p $(ZIB_CANVAS_DIR)
	@printf '%s\n' "$(ZIB_CANVAS_BUNDLE)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZIB_CANVAS_DIR)/toolchain: zib-canvas-config
	@mkdir -p $(ZIB_CANVAS_DIR)
	@printf '%s\n' "$(ZIRAN_DIR)" "$(ZIB_CANVAS_CC)" "$(ZIB_CANVAS_AR)" "$(ZIB_CANVAS_CACHE)" > $@.new
	@cmp -s $@.new $@ && rm $@.new || mv $@.new $@

$(ZIB_CANVAS_GENERATED)/.complete: $(ZIB_CANVAS_SOURCES) $(ZIB_CANVAS_COMPILER) $(KRYON_DIR)/mk/zib-canvas.mk $(ZIB_CANVAS_DIR)/toolchain
	mkdir -p $(ZIB_CANVAS_GENERATED)/ir $(ZIB_CANVAS_GENERATED)/c
	rm -f $(ZIB_CANVAS_GENERATED)/ir/*.zir $(ZIB_CANVAS_GENERATED)/c/*.[ch]
	$(ZIRAN_DIR)/build/bin/ziran ir --define PLATFORM_WEB --define BUNDLE_NATIVE_RENDERER --root $(KRYON_DIR)/tools \
		--module-path $(KRYON_DIR)/src/backend --module-path $(KRYON_DIR)/src/ui \
		--module-path $(ZIRAN_DIR)/std -o $(ZIB_CANVAS_GENERATED)/ir \
		$(KRYON_DIR)/tools/zib_canvas.zi $(KRYON_DIR)/src/backend/canvas_raster.zi
	$(ZIRAN_DIR)/build/bin/zi2c --define PLATFORM_WEB --define BUNDLE_NATIVE_RENDERER --root $(ZIB_CANVAS_GENERATED)/ir \
		-o $(ZIB_CANVAS_GENERATED)/c $(ZIB_CANVAS_GENERATED)/ir/*.zir
	touch $@

$(ZIB_CANVAS_RUNTIME): $(ZIB_CANVAS_RUNTIME_SOURCES) $(ZIB_CANVAS_DIR)/toolchain
	EM_CACHE=$(ZIB_CANVAS_CACHE) $(MAKE) -B -C $(ZIRAN_DIR) BOOTSTRAP=1 \
		BUILD_DIR=$(ZIB_CANVAS_DIR)/runtime CC=$(ZIB_CANVAS_CC) AR=$(ZIB_CANVAS_AR) \
		OBJCOPY=true CFLAGS=-O2 $(ZIB_CANVAS_RUNTIME)

$(ZIB_CANVAS_PROGRAM): $(ZIB_CANVAS_GENERATED)/.complete $(ZIB_CANVAS_RUNTIME) $(ZIB_CANVAS_BUNDLE) $(ZIB_CANVAS_DIR)/selected-bundle $(KRYON_DIR)/mk/zib-shell.html
	@mkdir -p $(dir $(ZIB_CANVAS_PROGRAM))
	EM_CACHE=$(ZIB_CANVAS_CACHE) $(ZIB_CANVAS_CC) -O2 -I$(ZIRAN_DIR)/include \
		-iquote $(ZIB_CANVAS_GENERATED)/c $(ZIB_CANVAS_GENERATED)/c/*.c $(ZIB_CANVAS_RUNTIME) \
		--js-library $(ZIRAN_DIR)/web/ziran_web.js -lidbfs.js -sEXPORTED_RUNTIME_METHODS=FS \
		--embed-file $(KRYON_DIR)/assets/fonts/LiberationSans-Regular.ttf@/kryon-font.ttf \
		$(if $(ZIB_CANVAS_BUNDLE),--embed-file "$(ZIB_CANVAS_BUNDLE)@/app.zib") \
		-sASYNCIFY -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=16777216 -sASYNCIFY_STACK_SIZE=1048576 \
		-sEXIT_RUNTIME=1 -sENVIRONMENT=web --shell-file $(KRYON_DIR)/mk/zib-shell.html \
		-o $(ZIB_CANVAS_PROGRAM)
