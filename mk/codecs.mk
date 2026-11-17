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

.PHONY: codec-archives
codec-archives: $(CODEC_ARCHIVES)

$(OPUS_A): $$(OPUS_INPUTS)
	cmake -S $(AUDIO_CODEC_PACKAGE)/native -B $(OPUS_BUILD) -DOPUS_DIR="$(OPUS_PACKAGE)" \
		-DSAMPLERATE_DIR="$(SAMPLERATE_PACKAGE)" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(CODEC_CC)"
	command cmake --build $(OPUS_BUILD) --target opus --parallel $(CODEC_JOBS)
	@test -f $@

# `command` runs cmake through the shell: GNU Make 4.3's own search stops at the
# cmake folder Emscripten puts on PATH and fails with Permission denied.
$(WEBP_A): $$(WEBP_INPUTS)
	@test -n "$(WEBP_PACKAGE)" && test -f "$(WEBP_PACKAGE)/src/demux/anim_decode.c" || { echo "Run ziran fetch; Kryon needs the locked libwebp source package" >&2; exit 1; }
	cmake -S $(KRYON_DIR)/cmake/webp -B $(WEBP_BUILD) -DWEBP_DIR="$(WEBP_PACKAGE)" \
		-DKRYON_WEBP_PACKAGE_DIR="$(WEBP_CODEC_PACKAGE)" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(CODEC_CC)" \
		-DCMAKE_C_FLAGS="$(WEBP_CFLAGS)" >/dev/null
	command cmake --build $(WEBP_BUILD) --target kryonwebp --parallel $(CODEC_JOBS)
	@test -f $@

$(AVIF_A): $$(AVIF_INPUTS)
	cmake -S $(KRYON_DIR)/cmake/avif -B $(AVIF_BUILD) -DAVIF_DIR="$(AVIF_PACKAGE)" -DAOM_DIR="$(AOM_PACKAGE)" \
		-DDAV1D_DIR="$(DAV1D_PACKAGE)" -DKRYON_AVIF_PACKAGE_DIR="$(AVIF_CODEC_PACKAGE)" \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$(CODEC_CC)"
	command cmake --build $(AVIF_BUILD) --target avif_static $(if $(filter Linux,$(CODEC_HOST_OS)),kryonavif) --parallel $(CODEC_JOBS)
	@test -f $@

$(AOM_A) $(AVIF_SO): $(AVIF_A)
	@test -f $@

endif
