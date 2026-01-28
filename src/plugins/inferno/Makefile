# Makefile for Inferno Plugin
# This file is included by the main Makefile when building for Inferno

# Plugin sources
INFERNO_PLUGIN_SRC = \
	src/plugins/inferno/plugin.c \
	src/plugins/inferno/extended_file_io.c \
	src/plugins/inferno/namespace.c \
	src/plugins/inferno/process_control.c

# Plugin objects (in build directory)
INFERNO_PLUGIN_OBJ = $(INFERNO_PLUGIN_SRC:src/%.c=$(OBJ_DIR)/%.o)

# Plugin flags
INFERNO_PLUGIN_CFLAGS = -DKRYON_PLUGIN_INFERNO

# Add to main build
PLUGIN_SRC += $(INFERNO_PLUGIN_SRC)
PLUGIN_OBJ += $(INFERNO_PLUGIN_OBJ)
PLUGIN_CFLAGS += $(INFERNO_PLUGIN_CFLAGS)

# Inferno/TaijiOS lib9 linkage (if INFERNO environment variable is set)
ifdef INFERNO
# Auto-detect architecture (amd64 or 386)
INFERNO_ARCH := $(shell uname -m | sed 's/x86_64/amd64/;s/i686/386/')

# Check for lib9.a to determine correct path
ifneq ($(wildcard $(INFERNO)/Linux/$(INFERNO_ARCH)/lib/lib9.a),)
PLUGIN_LDFLAGS += -L$(INFERNO)/Linux/$(INFERNO_ARCH)/lib -l9 -lbio
PLUGIN_INCLUDES += -I$(INFERNO)/include
else ifneq ($(wildcard $(INFERNO)/Linux/386/lib/lib9.a),)
PLUGIN_LDFLAGS += -L$(INFERNO)/Linux/386/lib -l9 -lbio
PLUGIN_INCLUDES += -I$(INFERNO)/include
else
$(warning Could not find lib9.a in $(INFERNO)/Linux/*/lib)
endif
endif
