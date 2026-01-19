# Codegen Makefile Template
# =========================
# Shared makefile rules for all code generators
# Usage: Include this from your codegen's Makefile

# Get codegen name from directory name (e.g., "c", "lua", "kry")
CODEGEN := $(notdir $(CURDIR))

# Source files
SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)

# Output library names
BUILD_DIR := ../../build
STATIC_LIB = $(BUILD_DIR)/libkryon_codegen_$(CODEGEN).a
SHARED_LIB = $(BUILD_DIR)/libkryon_codegen_$(CODEGEN).so

# Include paths
CPPFLAGS := -I../../include -I../../third_party/cJSON

# Linker flags
LDFLAGS := -L../../build -lkryon_ir

# Default target
all: $(STATIC_LIB)

# Build static library only (most codegens don't need shared)
static: $(STATIC_LIB)

# Build shared library (optional)
shared: $(SHARED_LIB)

# Build static library
$(STATIC_LIB): $(OBJECTS) | $(BUILD_DIR)
	$(AR) rcs $@ $^
	@echo "  Built $(CODEGEN) codegen: $@"

# Build shared library
$(SHARED_LIB): $(OBJECTS) | $(BUILD_DIR)
	$(CC) -shared -o $@ $^ $(LDFLAGS)
	@echo "  Built $(CODEGEN) codegen (shared): $@"

# Create build directory
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Clean
clean:
	rm -f $(OBJECTS) $(STATIC_LIB) $(SHARED_LIB)

# Dependencies
CFLAGS += -MMD -MP
DEPS := $(OBJECTS:.o=.d)
-include $(DEPS)

.PHONY: all static shared clean
