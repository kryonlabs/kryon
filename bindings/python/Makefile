# Python Bindings Makefile
# ========================

# Build directory
BUILD_DIR := ../../build
PYTHON_BUILD_DIR := $(BUILD_DIR)/python

# Default target
all: package

# Create build directories
$(PYTHON_BUILD_DIR):
	@mkdir -p $(PYTHON_BUILD_DIR)

# Build Python package (uses pyproject.toml)
package: $(PYTHON_BUILD_DIR)
	@echo "Building Python bindings package..."
	@if [ -d "kryon" ]; then \
		python3 -m build --wheel --outdir $(PYTHON_BUILD_DIR) 2>/dev/null || \
		python3 build.py; \
	fi
	@echo "✓ Built Python bindings package"

# Install Python package
install: package
	@echo "Installing Python bindings..."
	@pip3 install -e .
	@echo "✓ Installed Python bindings"

# Run tests
test:
	@echo "Running Python tests..."
	@cd .. && python3 -m pytest bindings/python/tests/ -v
	@echo "✓ Tests complete"

# Clean
clean:
	@echo "Cleaning Python build artifacts..."
	@rm -rf $(PYTHON_BUILD_DIR)
	@rm -rf kryon.egg-info
	@rm -rf dist
	@find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.pyc" -delete 2>/dev/null || true
	@echo "✓ Clean complete"

.PHONY: all package install test clean
