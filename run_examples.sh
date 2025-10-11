#!/bin/bash

# Build and Test Script for Kryon Examples
# This script compiles and runs each example one by one

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Create build directory
BUILD_DIR="examples/build"
mkdir -p "$BUILD_DIR"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Kryon Examples Build & Test Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to compile an example
compile_example() {
    local example_file="$1"
    local example_name=$(basename "$example_file" .nim)
    local build_path="$BUILD_DIR/$example_name"

    echo -e "${BLUE}Compiling ${CYAN}$example_name${NC}..."
    if nim c -o:"$build_path" "$example_file"; then
        echo -e "${GREEN}✓ $example_name compiled successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ Compilation failed for $example_name${NC}"
        return 1
    fi
}

# Function to run a compiled example
run_example() {
    local example_name="$1"
    local build_path="$BUILD_DIR/$example_name"

    if [ -f "$build_path" ]; then
        echo -e "${PURPLE}Running: ${CYAN}$example_name${NC}"
        echo -e "${YELLOW}(Close window to continue automatically)${NC}"
        echo ""

        # Run the compiled example
        "$build_path"

        echo -e "${GREEN}✓ $example_name completed${NC}"
        echo -e "${BLUE}----------------------------------------${NC}"
        echo ""
        return 0
    else
        echo -e "${RED}✗ Executable not found for $example_name: $build_path${NC}"
        return 1
    fi
}

# Get all .nim files in examples directory
examples_dir="examples"
examples=($(find "$examples_dir" -name "*.nim" -not -path "*/build/*" | sort))

if [ ${#examples[@]} -eq 0 ]; then
    echo -e "${RED}No .nim files found in $examples_dir${NC}"
    exit 1
fi

echo -e "${CYAN}Found ${#examples[@]} examples to test:${NC}"
for example in "${examples[@]}"; do
    echo -e "  - $(basename "$example" .nim)"
done
echo ""

# Counter for tracking progress
total_examples=${#examples[@]}
current_example=0

# Process each example
for example_file in "${examples[@]}"; do
    current_example=$((current_example + 1))

    echo -e "${BLUE}[${current_example}/${total_examples}]${NC} Starting build process..."

    if build_and_run_example "$example_file"; then
        echo -e "${GREEN}=== SUCCESS: $(basename "$example_file" .nim) ===${NC}"
    else
        echo -e "${RED}=== FAILED: $(basename "$example_file" .nim) ===${NC}"
    fi

    echo ""
done

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All examples processed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${CYAN}Build files are located in: $BUILD_DIR${NC}"
echo ""

# Show summary of what was built
echo -e "${YELLOW}Summary of built examples:${NC}"
for build_file in "$BUILD_DIR"/*; do
    if [ -f "$build_file" ]; then
        echo -e "  ${GREEN}✓${NC} $(basename "$build_file")"
    fi
done