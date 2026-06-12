FLINT_MAKE_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
FLINT_PROJECT ?= flint.toml
FLINT_BIN ?= flint
FLINT_PROJECT_VARS ?= build/flint-project.mk

ifeq ($(wildcard $(FLINT_PROJECT)),)
$(error missing $(FLINT_PROJECT))
endif

$(FLINT_PROJECT_VARS): $(FLINT_PROJECT)
	@mkdir -p $(dir $@)
	$(FLINT_BIN) make-vars > $@

-include $(FLINT_PROJECT_VARS)

include $(FLINT_MAKE_DIR)common.mk
include $(FLINT_MAKE_DIR)native.mk
include $(FLINT_MAKE_DIR)windows.mk
include $(FLINT_MAKE_DIR)web.mk
include $(FLINT_MAKE_DIR)android.mk
include $(FLINT_MAKE_DIR)dist.mk
include $(FLINT_MAKE_DIR)clean.mk

FORCE:

.PHONY: \
	all \
	native \
	run \
	linux \
	$(LINUX_ARCHES:%=linux-%) \
	build-linux-arch \
	windows \
	web \
	clean \
	clean-linux \
	clean-windows \
	clean-web \
	clean-raylib \
	dist \
	dist-linux \
	dist-windows \
	install \
	uninstall \
	android-init-signing \
	android-debug \
	android-release \
	android-bundle \
	android-copy-assets \
	android-copy-apks \
	android-copy-debug-apks \
	android-copy-release-apks \
	android-copy-bundle \
	android-install \
	android-install-release \
	android-clean \
	FORCE
