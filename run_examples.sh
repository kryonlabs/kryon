#!/bin/bash

# Build and Test Script for Kryon Examples
# This script compiles the kryon CLI first, compiles all examples, then runs them

set -e  # Exit on any error (but not on Ctrl+C during example runs)

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# CLI paths
CLI_PATH="./bin/cli/kryon"
CLI_SOURCE="src/cli/kryon.nim"

# Create build and bin directories if they don't exist
BUILD_DIR="examples/build"
mkdir -p "$(dirname "$CLI_PATH")"
mkdir -p "$BUILD_DIR"

echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  Kryon Examples Build & Test Script${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

# Function to compile the kryon CLI
compile_cli() {
    echo -e "${BLUE}Compiling Kryon CLI...${NC}"
    if nim c --parallelBuild:$(nproc) -o:"$CLI_PATH" "$CLI_SOURCE"; then
        echo -e "${GREEN}✓ Kryon CLI compiled successfully${NC}"
        return 0
    else
        echo -e "${RED}✗ Kryon CLI compilation failed${NC}"
        return 1
    fi
}

# Function to compile an example using the kryon CLI
compile_example() {
    local example_file="$1"
    local example_name=$(basename "$example_file" .nim)
    local build_path="$BUILD_DIR/$example_name"

    echo -e "${BLUE}Compiling ${CYAN}$example_name${NC}..."
    if "$CLI_PATH" build -f "$example_file" -r raylib -o "$build_path"; then
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
        echo -e "${YELLOW}(Close window or press Ctrl+C to continue automatically)${NC}"
        echo ""

        # Run the compiled example - allow immediate Ctrl+C
        "$build_path" || true

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

# Phase 1: Compile the kryon CLI
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PHASE 1: COMPILING KRYON CLI${NC}"
echo -e "${BLUE}========================================${NC}"
echo ""

if compile_cli; then
    echo -e "${GREEN}✓ CLI ready for use${NC}"
else
    echo -e "${RED}✗ Failed to compile CLI - cannot continue${NC}"
    exit 1
fi

echo ""

# Phase 2: Compile all examples
echo -e "${BLUE}========================================${NC}"
echo -e "${BLUE}  PHASE 2: COMPILING ALL EXAMPLES${NC}"
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

# Phase 3: Run all successfully compiled examples
if [ ${#successful_compilations[@]} -gt 0 ]; then
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  PHASE 3: RUNNING ALL EXAMPLES${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo ""

    # Disable 'set -e' during example runs to allow Ctrl+C to work
    set +e

    failed_runs=()
    successful_runs=()

    counter=0
    for example_name in "${successful_compilations[@]}"; do
        echo -e "${GREEN}[$((counter + 1))/${#successful_compilations[@]}]${NC} "
        if run_example "$example_name"; then
            successful_runs+=("$example_name")
            echo -e "${GREEN}=== SUCCESS: $example_name ===${NC}"
        else
            failed_runs+=("$example_name")
            echo -e "${RED}=== FAILED TO RUN: $example_name ===${NC}"
        fi
        counter=$((counter + 1))
        echo ""
    done

    # Re-enable 'set -e' after running all examples
    set -e

    # Run summary
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  RUN SUMMARY${NC}"
    echo -e "${BLUE}========================================${NC}"
    echo -e "${GREEN}Successfully ran: ${#successful_runs[@]}${NC}"
    for name in "${successful_runs[@]}"; do
        echo -e "  ${GREEN}✓${NC} $name"
    done

    if [ ${#failed_runs[@]} -gt 0 ]; then
        echo -e "${RED}Failed to run: ${#failed_runs[@]}${NC}"
        for name in "${failed_runs[@]}"; do
            echo -e "  ${RED}✗${NC} $name"
        done
    fi
else
    echo -e "${RED}No examples to run - all compilations failed!${NC}"
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}  All examples processed!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${CYAN}Kryon CLI is available at: $CLI_PATH${NC}"
echo -e "${CYAN}Build files are located in: $BUILD_DIR${NC}"
echo ""

# Show summary of what was built
echo -e "${YELLOW}Summary of built examples:${NC}"
for build_file in "$BUILD_DIR"/*; do
    if [ -f "$build_file" ]; then
        echo -e "  ${GREEN}✓${NC} $(basename "$build_file")"
    fi
done