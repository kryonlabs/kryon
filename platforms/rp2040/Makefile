# Makefile for RP2040 Platform Integration Kit

# Toolchain
CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# MCU Configuration
MCU = cortex-m0plus
ARCH = armv6-m
FPU =
FLOAT_ABI = soft

# Compiler flags
CFLAGS = -mcpu=$(MCU) -mthumb -march=$(ARCH)
CFLAGS += -Wall -Wextra -std=c99 -O2 -fdata-sections -ffunction-sections
CFLAGS += -DKRYON_PLATFORM_MCU -DKRYON_NO_HEAP -DKRYON_NO_FLOAT
CFLAGS += -DKRYON_RP2040_PICO_SDK_AVAILABLE

# Include paths
INCLUDES = -I../../core/include
INCLUDES += -I../../Pico-SDK/pico-sdk/src/common/pico_stdlib/include
INCLUDES += -I../../Pico-SDK/pico-sdk/src/rp2_common/hardware_base/include
INCLUDES += -I../../Pico-SDK/pico-sdk/src/rp2040/hardware_structs/include
INCLUDES += -I../../Pico-SDK/pico-sdk/src/common/pico_binary_info/include
INCLUDES += -I../../Pico-SDK/pico-sdk/src/boards/include

# Linker flags
LDFLAGS = -mcpu=$(MCU) -mthumb -march=$(ARCH)
LDFLAGS += -Wl,--gc-sections -Wl,--print-memory-usage

# Source files
SOURCES = rp2040_platform.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = libkryon_rp2040_platform.a

# Pico SDK environment
PICO_SDK_PATH ?= ../../Pico-SDK/pico-sdk

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Creating RP2040 static library: $(TARGET)"
	$(AR) rcs $(TARGET) $(OBJECTS)

%.o: %.c
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning RP2040 build artifacts..."
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	@echo "Installing RP2040 library to ../../build/"
	cp $(TARGET) ../../build/

# Size analysis
size: $(TARGET)
	@echo "=== Library Size Analysis ==="
	$(SIZE) $(TARGET)
	@echo "============================="

# Check Pico SDK
check-sdk:
	@if [ ! -d "$(PICO_SDK_PATH)" ]; then \
		echo "Pico SDK not found at $(PICO_SDK_PATH)"; \
		echo "Please clone Pico SDK to ../../Pico-SDK/"; \
		exit 1; \
	else \
		echo "Pico SDK found at $(PICO_SDK_PATH)"; \
	fi

# Help target
help:
	@echo "RP2040 Platform Integration Kit Makefile"
	@echo "========================================="
	@echo "Available targets:"
	@echo "  all      - Build the RP2040 platform library"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install library to main build directory"
	@echo "  size     - Show library size information"
	@echo "  check-sdk - Verify Pico SDK availability"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  MCU: $(MCU)"
	@echo "  ARCH: $(ARCH)"
	@echo "  PICO_SDK_PATH: $(PICO_SDK_PATH)"

.PHONY: all clean install size check-sdk help