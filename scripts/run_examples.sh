#!/usr/bin/env bash

# run_examples.sh - Run all Kryon examples with specified renderer
# Usage:
#   ./run_examples.sh                - Run all examples with raylib renderer
#   ./run_examples.sh raylib         - Run all examples with raylib renderer
#   ./run_examples.sh text           - Run all examples with text renderer
#   ./run_examples.sh 1              - Skip first 1 examples, start from example 2
#   ./run_examples.sh raylib 1       - Run with raylib renderer, skip first 1 examples
#   ./run_examples.sh list           - List all examples with their numbers

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default settings
RENDERER="raylib"
SKIP_COUNT=0
SHOW_LIST=false

# Parse command line arguments
for arg in "$@"; do
    if [ "$arg" = "list" ]; then
        SHOW_LIST=true
    elif [[ "$arg" =~ ^(raylib|sdl2|html|text|terminal)$ ]]; then
        RENDERER="$arg"
    elif [[ "$arg" =~ ^[0-9]+$ ]]; then
        SKIP_COUNT=$arg
    fi
done

# Show list if requested
if $SHOW_LIST; then
    echo "📋 KRY Examples List"
    echo "================================"
    
    # Find all .kry files and display them in order
    cd "$PROJECT_ROOT"
    kry_files=($(find examples -name "*.kry" -type f ! -path "*/debug/*" ! -path "*/templates/*" | sort))
    
    if [ ${#kry_files[@]} -eq 0 ]; then
        print_error "No .kry files found!"
        exit 1
    fi
    
    echo "Found ${#kry_files[@]} examples:"
    echo ""
    
    for i in "${!kry_files[@]}"; do
        echo "  [$((i+1))] ${kry_files[i]}"
    done
    
    echo ""
    echo "================================"
    exit 0
fi

echo "🚀 Kryon-C Examples Test Runner"
echo "================================"

if [ $SKIP_COUNT -gt 0 ]; then
    print_warning "Will skip first $SKIP_COUNT examples"
    echo ""
fi

# Build the kryon-c system
print_status "Building Kryon-C system..."
if ! "$SCRIPT_DIR/build.sh"; then
    print_error "Failed to build project"
    exit 1
fi

# Set paths
KRYON_PATH="$PROJECT_ROOT/build/bin/kryon"

# Verify binary was created
if [ ! -f "$KRYON_PATH" ]; then
    print_error "Kryon-C build failed: $KRYON_PATH not found"
    exit 1
fi

print_success "Kryon-C CLI: $KRYON_PATH"
print_success "Using renderer: $RENDERER"
echo ""

# Find all .kry files
cd "$PROJECT_ROOT"
kry_files=($(find examples -name "*.kry" -type f ! -path "*/debug/*" ! -path "*/templates/*" | sort))

if [ ${#kry_files[@]} -eq 0 ]; then
    print_error "No .kry files found!"
    exit 1
fi

# Apply skip count
if [ $SKIP_COUNT -gt 0 ]; then
    if [ $SKIP_COUNT -ge ${#kry_files[@]} ]; then
        print_error "Skip count ($SKIP_COUNT) is greater than or equal to total examples (${#kry_files[@]})"
        exit 1
    fi
    
    # Create new array with skipped elements
    skipped_files=("${kry_files[@]:$SKIP_COUNT}")
    kry_files=("${skipped_files[@]}")
fi

print_status "Found ${#kry_files[@]} .kry files to test:"
for i in "${!kry_files[@]}"; do
    actual_index=$((i + SKIP_COUNT + 1))
    echo "  [$actual_index] ${kry_files[i]}"
done
echo ""

# Run each .kry file with kryon-c binary
for i in "${!kry_files[@]}"; do
    kry_file="${kry_files[i]}"
    actual_index=$((i + SKIP_COUNT + 1))
    total_examples=${#kry_files[@]}
    
    echo "════════════════════════════════════════════════════════════════"
    echo "🎯 Running [$actual_index/$((total_examples + SKIP_COUNT))]: $kry_file"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "   📝 Description: $(basename "$kry_file" .kry | sed 's/_/ /g' | sed 's/\b\w/\U&/g')"
    echo "   🎮 Instructions: Review the widget output and layout"
    echo "   🚀 Running example..."
    
    sleep 1
    
    # Compile the KRY file to KRB
    krb_file="${kry_file%.kry}.krb"
    print_status "Compiling $kry_file to $krb_file..."
    
    if "$KRYON_PATH" compile "$kry_file" -o "$krb_file"; then
        print_success "Compilation successful"
        
        # Run the KRB file
        print_status "Running with $RENDERER renderer..."
        if "$KRYON_PATH" run "$krb_file" --renderer="$RENDERER"; then
            print_success "Completed: $kry_file"
        else
            print_error "Failed to run: $kry_file (exit code: $?)"
            echo "   Continue to next example? [y/N]"
            read -r response
            if [[ ! "$response" =~ ^[Yy]$ ]]; then
                echo "🛑 Stopping test run."
                exit 1
            fi
        fi
    else
        print_error "Failed to compile: $kry_file (exit code: $?)"
        echo "   Continue to next example? [y/N]"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            echo "🛑 Stopping test run."
            exit 1
        fi
    fi
    echo ""
done

echo "🎉 All examples completed!"
echo "================================"
echo "Summary:"
echo "  Total files tested: ${#kry_files[@]}"
echo "  Renderer used: $RENDERER"
echo "  Binary: $KRYON_PATH"