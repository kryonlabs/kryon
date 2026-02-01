# Kryon Codegens Makefile
# Builds code generation system with language and toolkit registries

CC = gcc
CFLAGS = -std=c99 -Wall -Wextra -O2 -fPIC -D_POSIX_C_SOURCE=200809L
AR = ar
ARFLAGS = rcs

# Directories
BUILD_DIR = ../build
CJSON_DIR = ../third_party/cJSON
IR_DIR = ../ir

# ============================================================================
# Core Registry Files
# ============================================================================

# Language Registry
LANGUAGE_REGISTRY_OBJ = $(BUILD_DIR)/language_registry.o
LANGUAGE_REGISTRY_SRC = languages/common/language_registry.c
LANGUAGE_REGISTRY_HDR = languages/common/language_registry.h

# Toolkit Registry
TOOLKIT_REGISTRY_OBJ = $(BUILD_DIR)/toolkit_registry.o
TOOLKIT_REGISTRY_SRC = toolkits/common/toolkit_registry.c
TOOLKIT_REGISTRY_HDR = toolkits/common/toolkit_registry.h

# Combo Registry
COMBO_REGISTRY_OBJ = $(BUILD_DIR)/combo_registry.o
COMBO_REGISTRY_SRC = combo_registry.c
COMBO_REGISTRY_HDR = combo_registry.h

# Codegen Target Helper
CODEGEN_TARGET_OBJ = $(BUILD_DIR)/codegen_target.o
CODEGEN_TARGET_SRC = codegen_target.c
CODEGEN_TARGET_HDR = codegen_target.h

# Codegen Common (for all codegens)
CODEGEN_COMMON_OBJ = $(BUILD_DIR)/codegen_common.o
CODEGEN_COMMON_SRC = codegen_common.c
CODEGEN_COMMON_HDR = codegen_common.h

# Unified Codegen Interface
CODEGEN_INTERFACE_OBJ = $(BUILD_DIR)/codegen_registry.o
CODEGEN_INTERFACE_SRC = codegen_registry.c
CODEGEN_INTERFACE_HDR = codegen_interface.h

# Codegen Registration (centralized registration)
CODEGEN_REGISTRATION_OBJ = $(BUILD_DIR)/codegen_registration.o
CODEGEN_REGISTRATION_SRC = codegen_registration.c

# ============================================================================
# Language Emitters
# ============================================================================

# Tcl Emitter
TCL_EMITTER_OBJ = $(BUILD_DIR)/tcl_emitter.o
TCL_EMITTER_SRC = languages/tcl/tcl_emitter.c
TCL_EMITTER_HDR = languages/tcl/tcl_emitter.h

# Limbo Emitter
LIMBO_EMITTER_OBJ = $(BUILD_DIR)/limbo_emitter.o
LIMBO_EMITTER_SRC = languages/limbo/limbo_emitter.c
LIMBO_EMITTER_HDR = languages/limbo/limbo_emitter.h

# ============================================================================
# Toolkit IRs
# ============================================================================

# DrawIR
DRAW_IR_BUILDER_OBJ = $(BUILD_DIR)/draw_ir_builder.o
DRAW_IR_BUILDER_SRC = toolkits/draw/draw_ir_builder.c
DRAW_IR_HDR = toolkits/draw/draw_ir.h

# ============================================================================
# Output Libraries
# ============================================================================

COMMON_LIB = $(BUILD_DIR)/libkryon_codegen_common.a
REGISTRY_LIB = $(BUILD_DIR)/libkryon_codegen_registry.a
EMITTER_LIB = $(BUILD_DIR)/libkryon_codegen_emitter.a
TOOLKIT_LIB = $(BUILD_DIR)/libkryon_codegen_toolkit.a

# All registry objects
REGISTRY_OBJS = $(LANGUAGE_REGISTRY_OBJ) \
               $(TOOLKIT_REGISTRY_OBJ) \
               $(COMBO_REGISTRY_OBJ) \
               $(CODEGEN_TARGET_OBJ)

# All emitter objects
EMITTER_OBJS = $(TCL_EMITTER_OBJ) \
               $(LIMBO_EMITTER_OBJ)

# All toolkit objects
TOOLKIT_OBJS = $(DRAW_IR_BUILDER_OBJ)

# ============================================================================
# Codegen Adapters (unified interface)
# ============================================================================

# KRY Codegen Adapter
KRY_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/kry_codegen_adapter.o
KRY_CODEGEN_ADAPTER_SRC = kry/kry_codegen_adapter.c

# C Codegen Adapter
C_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/c_codegen_adapter.o
C_CODEGEN_ADAPTER_SRC = c/c_codegen_adapter.c

# Limbo Codegen Adapter
LIMBO_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/limbo_codegen_adapter.o
LIMBO_CODEGEN_ADAPTER_SRC = limbo/limbo_codegen_adapter.c

# Tcl/Tk Codegen Adapter
TCLTK_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/tcltk_codegen_adapter.o
TCLTK_CODEGEN_ADAPTER_SRC = tcltk/tcltk_codegen_adapter.c

# Markdown Codegen Adapter
MARKDOWN_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/markdown_codegen_adapter.o
MARKDOWN_CODEGEN_ADAPTER_SRC = markdown/markdown_codegen_adapter.c

# Lua Codegen Adapter
LUA_CODEGEN_ADAPTER_OBJ = $(BUILD_DIR)/lua_codegen_adapter.o
LUA_CODEGEN_ADAPTER_SRC = lua/lua_codegen_adapter.c

# Lua Codegen
LUA_CODEGEN_OBJ = $(BUILD_DIR)/lua_codegen.o
LUA_CODEGEN_SRC = lua/lua_codegen.c
LUA_CODEGEN_HDR = lua/lua_codegen.h

# All codegen adapter objects
CODEGEN_ADAPTER_OBJS = $(KRY_CODEGEN_ADAPTER_OBJ) \
                      $(C_CODEGEN_ADAPTER_OBJ) \
                      $(LIMBO_CODEGEN_ADAPTER_OBJ) \
                      $(TCLTK_CODEGEN_ADAPTER_OBJ) \
                      $(MARKDOWN_CODEGEN_ADAPTER_OBJ) \
                      $(LUA_CODEGEN_ADAPTER_OBJ)

# All codegen implementation objects (for linking into common lib)
CODEGEN_IMPL_OBJS = $(LUA_CODEGEN_OBJ)

# ============================================================================
# Targets
# ============================================================================

# Default target - build all
all: $(BUILD_DIR) $(COMMON_LIB) $(REGISTRY_LIB) $(EMITTER_LIB) $(TOOLKIT_LIB)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ============================================================================
# Core Libraries
# ============================================================================

# Common utilities library (now includes unified codegen interface and adapters)
$(COMMON_LIB): $(CODEGEN_COMMON_OBJ) $(CODEGEN_INTERFACE_OBJ) $(CODEGEN_REGISTRATION_OBJ) $(CODEGEN_ADAPTER_OBJS) $(CODEGEN_IMPL_OBJS)
	$(AR) $(ARFLAGS) $@ $(CODEGEN_COMMON_OBJ) $(CODEGEN_INTERFACE_OBJ) $(CODEGEN_REGISTRATION_OBJ) $(CODEGEN_ADAPTER_OBJS) $(CODEGEN_IMPL_OBJS)

# Registry library
$(REGISTRY_LIB): $(REGISTRY_OBJS)
	$(AR) $(ARFLAGS) $@ $(REGISTRY_OBJS)

# Emitter library
$(EMITTER_LIB): $(EMITTER_OBJS)
	$(AR) $(ARFLAGS) $@ $(EMITTER_OBJS)

# Toolkit library
$(TOOLKIT_LIB): $(TOOLKIT_OBJS)
	$(AR) $(ARFLAGS) $@ $(TOOLKIT_OBJS)

# ============================================================================
# Object Files
# ============================================================================

# Codegen common
$(CODEGEN_COMMON_OBJ): $(CODEGEN_COMMON_SRC) $(CODEGEN_COMMON_HDR)
	$(CC) $(CFLAGS) -I$(IR_DIR) -I$(CJSON_DIR) -I. -c $(CODEGEN_COMMON_SRC) -o $@

# Language registry
$(LANGUAGE_REGISTRY_OBJ): $(LANGUAGE_REGISTRY_SRC) $(LANGUAGE_REGISTRY_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -c $(LANGUAGE_REGISTRY_SRC) -o $@

# Toolkit registry
$(TOOLKIT_REGISTRY_OBJ): $(TOOLKIT_REGISTRY_SRC) $(TOOLKIT_REGISTRY_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -c $(TOOLKIT_REGISTRY_SRC) -o $@

# Combo registry
$(COMBO_REGISTRY_OBJ): $(COMBO_REGISTRY_SRC) $(COMBO_REGISTRY_HDR) \
                      $(LANGUAGE_REGISTRY_HDR) $(TOOLKIT_REGISTRY_HDR)
	$(CC) $(CFLAGS) -I. -Ilanguages/common -Itoolkits/common -c $(COMBO_REGISTRY_SRC) -o $@

# Codegen target helper
$(CODEGEN_TARGET_OBJ): $(CODEGEN_TARGET_SRC) $(CODEGEN_TARGET_HDR) \
                      $(COMBO_REGISTRY_HDR) $(LANGUAGE_REGISTRY_HDR) $(TOOLKIT_REGISTRY_HDR)
	$(CC) $(CFLAGS) -I. -Ilanguages/common -Itoolkits/common -c $(CODEGEN_TARGET_SRC) -o $@

# Tcl emitter
$(TCL_EMITTER_OBJ): $(TCL_EMITTER_SRC) $(TCL_EMITTER_HDR) \
                   $(CODEGEN_COMMON_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -c $(TCL_EMITTER_SRC) -o $@

# Limbo emitter
$(LIMBO_EMITTER_OBJ): $(LIMBO_EMITTER_SRC) $(LIMBO_EMITTER_HDR) \
                     $(CODEGEN_COMMON_HDR) $(DRAW_IR_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -Itoolkits/draw -c $(LIMBO_EMITTER_SRC) -o $@

# DrawIR builder
$(DRAW_IR_BUILDER_OBJ): $(DRAW_IR_BUILDER_SRC) $(DRAW_IR_HDR) \
                       $(CODEGEN_COMMON_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -Itoolkits/draw -c $(DRAW_IR_BUILDER_SRC) -o $@

# ============================================================================
# Codegen Interface and Adapters
# ============================================================================

# Codegen registry
$(CODEGEN_INTERFACE_OBJ): $(CODEGEN_INTERFACE_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -c $(CODEGEN_INTERFACE_SRC) -o $@

# Codegen registration
$(CODEGEN_REGISTRATION_OBJ): $(CODEGEN_REGISTRATION_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(CJSON_DIR) -c $(CODEGEN_REGISTRATION_SRC) -o $@

# KRY codegen adapter
$(KRY_CODEGEN_ADAPTER_OBJ): $(KRY_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Ikry -c $(KRY_CODEGEN_ADAPTER_SRC) -o $@

# C codegen adapter
$(C_CODEGEN_ADAPTER_OBJ): $(C_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Ic -c $(C_CODEGEN_ADAPTER_SRC) -o $@

# Limbo codegen adapter
$(LIMBO_CODEGEN_ADAPTER_OBJ): $(LIMBO_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Ilimbo -c $(LIMBO_CODEGEN_ADAPTER_SRC) -o $@

# Tcl/Tk codegen adapter
$(TCLTK_CODEGEN_ADAPTER_OBJ): $(TCLTK_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Itcltk -c $(TCLTK_CODEGEN_ADAPTER_SRC) -o $@

# Markdown codegen adapter
$(MARKDOWN_CODEGEN_ADAPTER_OBJ): $(MARKDOWN_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Imarkdown -c $(MARKDOWN_CODEGEN_ADAPTER_SRC) -o $@

# Lua codegen adapter
$(LUA_CODEGEN_ADAPTER_OBJ): $(LUA_CODEGEN_ADAPTER_SRC) $(CODEGEN_INTERFACE_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(CJSON_DIR) -Ilua -c $(LUA_CODEGEN_ADAPTER_SRC) -o $@

# Lua codegen implementation
$(LUA_CODEGEN_OBJ): $(LUA_CODEGEN_SRC) $(LUA_CODEGEN_HDR) $(CODEGEN_COMMON_HDR)
	$(CC) $(CFLAGS) -I. -I$(IR_DIR)/include -I$(IR_DIR)/src/utils -I$(CJSON_DIR) -Ilua -c $(LUA_CODEGEN_SRC) -o $@

# ============================================================================
# Clean
# ============================================================================

clean:
	rm -f $(CODEGEN_COMMON_OBJ)
	rm -f $(CODEGEN_INTERFACE_OBJ) $(CODEGEN_REGISTRATION_OBJ)
	rm -f $(CODEGEN_ADAPTER_OBJS)
	rm -f $(CODEGEN_IMPL_OBJS)
	rm -f $(REGISTRY_OBJS) $(REGISTRY_LIB)
	rm -f $(EMITTER_OBJS) $(EMITTER_LIB)
	rm -f $(TOOLKIT_OBJS) $(TOOLKIT_LIB)
	rm -f $(COMMON_LIB)

# ============================================================================
# Install Headers
# ============================================================================

install: all
	mkdir -p $(BUILD_DIR)/include/kryon/codegen
	cp $(CODEGEN_COMMON_HDR) $(BUILD_DIR)/include/kryon/codegen/
	cp $(CODEGEN_TARGET_HDR) $(BUILD_DIR)/include/kryon/codegen/
	cp $(COMBO_REGISTRY_HDR) $(BUILD_DIR)/include/kryon/codegen/
	mkdir -p $(BUILD_DIR)/include/kryon/codegen/languages
	cp $(LANGUAGE_REGISTRY_HDR) $(BUILD_DIR)/include/kryon/codegen/languages/
	mkdir -p $(BUILD_DIR)/include/kryon/codegen/toolkits
	cp $(TOOLKIT_REGISTRY_HDR) $(BUILD_DIR)/include/kryon/codegen/toolkits/

.PHONY: all clean install
