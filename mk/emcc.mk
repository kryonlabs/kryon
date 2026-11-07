# EMCC names the Emscripten compiler by full path, so $(dir $(EMCC))emar finds
# its sibling tools. An explicit EMCC wins, then ~/emsdk, then emcc on PATH.
ifeq ($(origin EMCC),undefined)
EMCC_HOME_PATH := $(HOME)/emsdk/upstream/emscripten/emcc
EMCC = $(eval EMCC := $(if $(wildcard $(EMCC_HOME_PATH)),$(EMCC_HOME_PATH),$(or $(shell command -v emcc 2>/dev/null),$(EMCC_HOME_PATH))))$(EMCC)
endif
