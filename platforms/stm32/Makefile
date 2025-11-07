# Makefile for STM32 Platform Integration Kit

# Toolchain
CC = arm-none-eabi-gcc
AR = arm-none-eabi-ar
OBJCOPY = arm-none-eabi-objcopy
SIZE = arm-none-eabi-size

# MCU Configuration
MCU = cortex-m4
ARCH = armv7e-m
FPU = fpv4-sp-d16
FLOAT_ABI = hard

# Compiler flags
CFLAGS = -mcpu=$(MCU) -mthumb -march=$(ARCH) -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI)
CFLAGS += -Wall -Wextra -std=c99 -O2 -fdata-sections -ffunction-sections
CFLAGS += -DKRYON_PLATFORM_MCU -DKRYON_NO_HEAP -DKRYON_NO_FLOAT
CFLAGS += -DKRYON_STM32_HAL_AVAILABLE

# Include paths
INCLUDES = -I../../core/include -I../../../CMSIS/Device/ST/STM32F4xx/Include -I../../../STM32F4xx_HAL_Driver/Inc

# Linker flags
LDFLAGS = -mcpu=$(MCU) -mthumb -march=$(ARCH) -mfpu=$(FPU) -mfloat-abi=$(FLOAT_ABI)
LDFLAGS += -Wl,--gc-sections -Wl,--print-memory-usage

# Source files
SOURCES = stm32_platform.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target
TARGET = libkryon_stm32_platform.a

# Default target
all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo "Creating STM32 static library: $(TARGET)"
	$(AR) rcs $(TARGET) $(OBJECTS)

%.o: %.c
	@echo "Compiling: $<"
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	@echo "Cleaning STM32 build artifacts..."
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	@echo "Installing STM32 library to ../../build/"
	cp $(TARGET) ../../build/

# Size analysis
size: $(TARGET)
	@echo "=== Library Size Analysis ==="
	$(SIZE) $(TARGET)
	@echo "============================="

# Help target
help:
	@echo "STM32 Platform Integration Kit Makefile"
	@echo "======================================="
	@echo "Available targets:"
	@echo "  all      - Build the STM32 platform library"
	@echo "  clean    - Remove build artifacts"
	@echo "  install  - Install library to main build directory"
	@echo "  size     - Show library size information"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  MCU: $(MCU)"
	@echo "  ARCH: $(ARCH)"
	@echo "  FPU: $(FPU)"
	@echo "  FLOAT-ABI: $(FLOAT_ABI)"

.PHONY: all clean install size help