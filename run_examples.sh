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
    if nim c --parallelBuild:1 -o:"$build_path" "$example_file"; then
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

# Phase 1: Compile all examples
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PHASE 1: COMPILING ALL EXAMPLES${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

failed_compilations=()
successful_compilations=()

for example_file in "${examples[@]}"; do
    current_example=$((current_example + 1))
    example_name=$(basename "$example_file" .nim)

    echo -e "${YELLOW}[${current_example}/${total_examples}]${NC} "

    if compile_example "$example_file"; then
        successful_compilations+=("$example_name")
    else
        failed_compilations+=("$example_name")
    fi
done

# Compilation summary
echo ""
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  COMPILATION SUMMARY${NC}"
echo -e "${BLUE}========================================${NC}"
echo -e "${GREEN}Successfully compiled: ${#successful_compilations[@]}${NC}"
for name in "${successful_compilations[@]}"; do
    echo -e "  ${GREEN}✓${NC} $name"
done

if [ ${#failed_compilations[@]} -gt 0 ]; then
    echo -e "${RED}Failed to compile: ${#failed_compilations[@]}${NC}"
    for name in "${failed_compilations[@]}"; do
        echo -e "  ${RED}✗${NC} $name"
    done
fi

echo ""

# Phase 2: Run all successfully compiled examples
if [ ${#successful_compilations[@]} -gt 0 ]; then
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  PHASE 2: RUNNING ALL EXAMPLES${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    counter=0
    for example_name in "${successful_compilations[@]}"; do
        echo -e "${GREEN}[$((counter + 1))/${#successful_compilations[@]}]${NC} "
        if run_example "$example_name"; then
            echo -e "${GREEN}=== SUCCESS: $example_name ===${NC}"
        else
            echo -e "${RED}=== FAILED TO RUN: $example_name ===${NC}"
        fi
        counter=$((counter + 1))
    done
else
    echo -e "${RED}No examples to run - all compilations failed!${NC}"
fi

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