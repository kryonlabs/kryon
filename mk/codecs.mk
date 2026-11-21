# Package-owned codec archives, independent of any window or rendering host.
ifndef KRYON_CODEC_RULES
KRYON_CODEC_RULES := 1
CODEC_CC ?= $(CC)
CODEC_JOBS ?= 4
CODEC_HOST_OS := $(shell uname -s)
WEBP_CFLAGS ?= -O2
.SECONDEXPANSION:
# Package-owned decoder recipes keep Kryon's existing archive paths.
WEBP_CODEC_PACKAGE = $(eval WEBP_CODEC_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path webp $(if $(wildcard ziran.local.toml),,--locked)))$(WEBP_CODEC_PACKAGE)
WEBP_PACKAGE = $(eval WEBP_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libwebp $(if $(wildcard ziran.local.toml),,--locked)))$(WEBP_PACKAGE)
WEBP_BUILD ?= $(KRYON_DIR)/build/webp-ziran
WEBP_A ?= $(WEBP_BUILD)/libkryonwebp.a
WEBP_INPUTS = $(KRYON_DIR)/mk/codecs.mk $(KRYON_DIR)/cmake/WebP.cmake $(KRYON_DIR)/cmake/ImagePackages.cmake $(KRYON_DIR)/cmake/webp/CMakeLists.txt $(wildcard $(WEBP_CODEC_PACKAGE)/native/*.cmake $(WEBP_PACKAGE)/CMakeLists.txt $(WEBP_PACKAGE)/src/*/*.c $(WEBP_PACKAGE)/src/*/*.h)
AVIF_CODEC_PACKAGE = $(eval AVIF_CODEC_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path avif $(if $(wildcard ziran.local.toml),,--locked)))$(AVIF_CODEC_PACKAGE)
AVIF_PACKAGE = $(eval AVIF_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libavif $(if $(wildcard ziran.local.toml),,--locked)))$(AVIF_PACKAGE)
AOM_PACKAGE = $(eval AOM_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libaom $(if $(wildcard ziran.local.toml),,--locked)))$(AOM_PACKAGE)
DAV1D_PACKAGE = $(eval DAV1D_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path dav1d $(if $(wildcard ziran.local.toml),,--locked)))$(DAV1D_PACKAGE)
AVIF_BUILD ?= $(KRYON_DIR)/build/avif-ziran
AVIF_A ?= $(AVIF_BUILD)/avif/libavif.a
AOM_A ?= $(AVIF_BUILD)/aom/libaom.a
AVIF_SO ?= $(AVIF_BUILD)/libkryonavif.so
AVIF_INPUTS = $(KRYON_DIR)/mk/codecs.mk $(KRYON_DIR)/cmake/AVIF.cmake $(KRYON_DIR)/cmake/ImagePackages.cmake $(KRYON_DIR)/cmake/avif/CMakeLists.txt $(wildcard $(AVIF_CODEC_PACKAGE)/native/*.cmake)

AUDIO_CODEC_PACKAGE = $(eval AUDIO_CODEC_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path audio $(if $(wildcard ziran.local.toml),,--locked)))$(AUDIO_CODEC_PACKAGE)
OPUS_PACKAGE = $(eval OPUS_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libopus $(if $(wildcard ziran.local.toml),,--locked)))$(OPUS_PACKAGE)
SAMPLERATE_PACKAGE = $(eval SAMPLERATE_PACKAGE := $(shell $(or $(ZIRAN),ziran) pkg path libsamplerate $(if $(wildcard ziran.local.toml),,--locked)))$(SAMPLERATE_PACKAGE)
OPUS_BUILD ?= $(KRYON_DIR)/build/opus
OPUS_A ?= $(OPUS_BUILD)/libopus/libopus.a
OPUS_INPUTS = $(KRYON_DIR)/mk/codecs.mk $(wildcard $(AUDIO_CODEC_PACKAGE)/native/*.cmake $(AUDIO_CODEC_PACKAGE)/native/CMakeLists.txt $(OPUS_PACKAGE)/CMakeLists.txt)
CODEC_ARCHIVES = $(WEBP_A) $(AVIF_A) $(AOM_A) $(OPUS_A)

.PHONY: codec-archives codec-config-force
codec-archives: $(CODEC_ARCHIVES)

# A suite can resolve the live packages first and installed immutable packages
# later. Track source identities independently of their file timestamps.
$(OPUS_BUILD)/kryon-config.json: codec-config-force
	@python3 $(KRYON_DIR)/scripts/codec_configure.py "$(OPUS_BUILD)" \
		"source=$(AUDIO_CODEC_PACKAGE)/native" "compiler=$(CODEC_CC)" \
		"opus=$(OPUS_PACKAGE)" "samplerate=$(SAMPLERATE_PACKAGE)"
$(WEBP_BUILD)/kryon-config.json: codec-config-force
	@python3 $(KRYON_DIR)/scripts/codec_configure.py "$(WEBP_BUILD)" \
		"source=$(KRYON_DIR)/cmake/webp" "compiler=$(CODEC_CC)" \
		"webp=$(WEBP_PACKAGE)" "package=$(WEBP_CODEC_PACKAGE)" "flags=$(WEBP_CFLAGS)"
$(OPUS_A): $$(OPUS_INPUTS) $(OPUS_BUILD)/kryon-config.json $(KRYON_DIR)/scripts/codec_configure.py
	cmake -S $(AUDIO_CODEC_PACKAGE)/native -B $(OPUS_BUILD) -DOPUS_DIR="$(OPUS_PACKAGE)" \
		-DSAMPLERATE_DIR="$(SAMPLERATE_PACKAGE)" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(CODEC_CC)"
	command cmake --build $(OPUS_BUILD) --target opus --parallel $(CODEC_JOBS)
	@test -f $@

# `command` runs cmake through the shell: GNU Make 4.3's own search stops at the
# cmake folder Emscripten puts on PATH and fails with Permission denied.
$(WEBP_A): $$(WEBP_INPUTS) $(WEBP_BUILD)/kryon-config.json $(KRYON_DIR)/scripts/codec_configure.py
	@test -n "$(WEBP_PACKAGE)" && test -f "$(WEBP_PACKAGE)/src/demux/anim_decode.c" || { echo "Run ziran fetch; Kryon needs the locked libwebp source package" >&2; exit 1; }
	cmake -S $(KRYON_DIR)/cmake/webp -B $(WEBP_BUILD) -DWEBP_DIR="$(WEBP_PACKAGE)" \
		-DKRYON_WEBP_PACKAGE_DIR="$(WEBP_CODEC_PACKAGE)" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(CODEC_CC)" \
		-DCMAKE_C_FLAGS="$(WEBP_CFLAGS)" >/dev/null
	command cmake --build $(WEBP_BUILD) --target kryonwebp --parallel $(CODEC_JOBS)
	@test -f $@

# Always check freshness inside the build-directory lock. Downstream make
# processes can share this directory even when their project roots differ.
$(AVIF_BUILD)/kryon-built: codec-config-force
	python3 $(KRYON_DIR)/scripts/build_avif.py "$(AVIF_BUILD)" --jobs "$(CODEC_JOBS)" \
		--archive "$(AVIF_A)" --aom-archive "$(AOM_A)" \
		$(if $(filter Linux,$(CODEC_HOST_OS)),--shared-library "$(AVIF_SO)") \
		$(foreach source,$(AVIF_INPUTS) $(KRYON_DIR)/scripts/codec_configure.py $(KRYON_DIR)/scripts/build_avif.py,--input "$(source)") \
		"source=$(KRYON_DIR)/cmake/avif" "compiler=$(CODEC_CC)" \
		"avif=$(AVIF_PACKAGE)" "aom=$(AOM_PACKAGE)" \
		"dav1d=$(DAV1D_PACKAGE)" "package=$(AVIF_CODEC_PACKAGE)"

# Make must recheck library mtimes after the shared build replaces them.
$(AVIF_A) $(AOM_A) $(AVIF_SO): $(AVIF_BUILD)/kryon-built
	@test -f $@

endif
