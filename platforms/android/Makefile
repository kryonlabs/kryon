# Kryon Android Platform Makefile
# This Makefile provides a wrapper around CMake for Android NDK builds

# Android NDK Configuration
NDK_ROOT ?= $(ANDROID_NDK_HOME)
BUILD_DIR = build
INSTALL_PREFIX ?= $(BUILD_DIR)/install

# Android ABI targets
ABIS = arm64-v8a armeabi-v7a x86_64 x86
DEFAULT_ABI = arm64-v8a

# API Level
API_LEVEL ?= 24

# Build type
BUILD_TYPE ?= Release

.PHONY: all clean help ndk-build install

all: ndk-build

help:
	@echo "Kryon Android Platform Build"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Build for default ABI ($(DEFAULT_ABI))"
	@echo "  ndk-build        - Build for all ABIs"
	@echo "  ndk-build-single - Build for DEFAULT_ABI only"
	@echo "  clean            - Clean build directory"
	@echo "  install          - Install libraries"
	@echo ""
	@echo "Configuration:"
	@echo "  NDK_ROOT         - Path to Android NDK ($(NDK_ROOT))"
	@echo "  API_LEVEL        - Android API level ($(API_LEVEL))"
	@echo "  BUILD_TYPE       - Release or Debug ($(BUILD_TYPE))"
	@echo "  DEFAULT_ABI      - Target ABI ($(DEFAULT_ABI))"
	@echo ""
	@echo "Examples:"
	@echo "  make ndk-build                    # Build all ABIs"
	@echo "  make ndk-build-single             # Build arm64-v8a only"
	@echo "  make API_LEVEL=28 ndk-build     # Build for API 28"
	@echo "  make BUILD_TYPE=Debug ndk-build  # Debug build"

ndk-build:
	@if [ -z "$(NDK_ROOT)" ]; then \
		echo "Error: Android NDK not found. Set NDK_ROOT or ANDROID_NDK_HOME"; \
		exit 1; \
	fi
	@echo "Building for all ABIs..."
	@for abi in $(ABIS); do \
		echo ""; \
		echo "=== Building for $$abi ==="; \
		mkdir -p $(BUILD_DIR)/$$abi; \
		(cd $(BUILD_DIR)/$$abi && \
		cmake ../.. \
			-DCMAKE_TOOLCHAIN_FILE=$(NDK_ROOT)/build/cmake/android.toolchain.cmake \
			-DANDROID_ABI=$$abi \
			-DANDROID_PLATFORM=android-$(API_LEVEL) \
			-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
			-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) && \
		cmake --build . --config $(BUILD_TYPE) -- -j8) || exit 1; \
	done
	@echo ""
	@echo "Build complete for all ABIs"

ndk-build-single:
	@if [ -z "$(NDK_ROOT)" ]; then \
		echo "Error: Android NDK not found. Set NDK_ROOT or ANDROID_NDK_HOME"; \
		exit 1; \
	fi
	@echo "Building for $(DEFAULT_ABI)..."
	@mkdir -p $(BUILD_DIR)/$(DEFAULT_ABI)
	@cd $(BUILD_DIR)/$(DEFAULT_ABI) && \
	cmake ../.. \
		-DCMAKE_TOOLCHAIN_FILE=$(NDK_ROOT)/build/cmake/android.toolchain.cmake \
		-DANDROID_ABI=$(DEFAULT_ABI) \
		-DANDROID_PLATFORM=android-$(API_LEVEL) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE) \
		-DCMAKE_INSTALL_PREFIX=$(INSTALL_PREFIX) && \
	cmake --build . --config $(BUILD_TYPE) -- -j8
	@echo "Build complete for $(DEFAULT_ABI)"

install:
	@echo "Installing libraries..."
	@for abi in $(ABIS); do \
		if [ -d "$(BUILD_DIR)/$$abi" ]; then \
			echo "Installing $$abi..."; \
			cd $(BUILD_DIR)/$$abi && cmake --install .; \
		fi; \
	done
	@echo "Installation complete"

clean:
	@echo "Cleaning build directory..."
	@rm -rf $(BUILD_DIR)
	@echo "Clean complete"

# Check NDK installation
check-ndk:
	@if [ -z "$(NDK_ROOT)" ]; then \
		echo "Error: Android NDK not found"; \
		echo "Please set NDK_ROOT or ANDROID_NDK_HOME environment variable"; \
		exit 1; \
	fi
	@echo "Android NDK: $(NDK_ROOT)"
	@echo "API Level: $(API_LEVEL)"
	@echo "Build Type: $(BUILD_TYPE)"
	@echo "Target ABIs: $(ABIS)"

# Size analysis
size: ndk-build
	@echo "Library sizes:"
	@for abi in $(ABIS); do \
		if [ -f "$(BUILD_DIR)/$$abi/libkryon_android_platform.a" ]; then \
			echo "$$abi:"; \
			ls -lh $(BUILD_DIR)/$$abi/libkryon_android_platform.a | awk '{print "  " $$9 ": " $$5}'; \
		fi; \
	done
