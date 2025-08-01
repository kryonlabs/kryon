#!/usr/bin/env bash

# Test script for running all KRY examples with the kryon-c system
# Automatically builds the system and runs examples with the appropriate renderer
#
# Usage:
#   ./run_examples.sh                - Run all examples with raylib renderer
#   ./run_examples.sh raylib         - Run all examples with raylib renderer
#   ./run_examples.sh sdl2           - Run all examples with sdl2 renderer
#   ./run_examples.sh html           - Run all examples with html renderer
#   ./run_examples.sh 1              - Skip first 1 examples, start from example 2
#   ./run_examples.sh raylib 1       - Run with raylib renderer, skip first 1 examples
#   ./run_examples.sh list           - List all examples with their numbers

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default renderer
RENDERER="raylib"
SKIP_COUNT=0
SHOW_LIST=false
DEBUG_MODE=false

# Parse command line arguments
for arg in "$@"; do
    if [ "$arg" = "list" ]; then
        SHOW_LIST=true
    elif [ "$arg" = "--debug" ]; then
        DEBUG_MODE=true
    elif [[ "$arg" =~ ^(raylib|sdl2|html|all|text)$ ]]; then
        RENDERER="$arg"
    elif [[ "$arg" =~ ^[0-9]+$ ]]; then
        SKIP_COUNT=$arg
    fi
done

# Set the appropriate binary path and renderer flag
KRYON_PATH="$PROJECT_ROOT/build/bin/kryon"
BUILD_DIR="$PROJECT_ROOT/build"

# Show list if requested
if $SHOW_LIST; then
    echo "📋 KRY Examples List"
    echo "================================"
    
    # Find all .kry files and display them in order
    cd "$PROJECT_ROOT"
    kry_files=($(find examples -name "*.kry" -type f ! -path "*/widgets/*" ! -path "*/templates/*" | sort))
    
    if [ ${#kry_files[@]} -eq 0 ]; then
        echo "❌ No .kry files found!"
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
    echo "⏭️  Will skip first $SKIP_COUNT examples"
    echo ""
fi

# Build the kryon-c system
echo "🔧 Building Kryon-C system..."
cd "$PROJECT_ROOT"

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
echo "  🔧 Configuring CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build the system
echo "  📦 Building kryon-c minimal widget system..."
make -j$(nproc)

# Update binary path for modular kryon CLI
KRYON_PATH="$BUILD_DIR/bin/kryon"

# Verify binary was created
if [ ! -f "$KRYON_PATH" ]; then
    echo "❌ Kryon-C build failed: $KRYON_PATH not found"
    exit 1
fi

echo "✅ Kryon-C CLI: $KRYON_PATH"
echo "✅ Using renderer: $RENDERER"
echo ""

# Find all .kry files
cd "$PROJECT_ROOT"
kry_files=($(find examples -name "*.kry" -type f ! -path "*/widgets/*" ! -path "*/templates/*" | sort))

if [ ${#kry_files[@]} -eq 0 ]; then
    echo "❌ No .kry files found!"
    exit 1
fi

# Apply skip count
if [ $SKIP_COUNT -gt 0 ]; then
    if [ $SKIP_COUNT -ge ${#kry_files[@]} ]; then
        echo "❌ Skip count ($SKIP_COUNT) is greater than or equal to total examples (${#kry_files[@]})"
        exit 1
    fi
    
    # Create new array with skipped elements
    skipped_files=("${kry_files[@]:$SKIP_COUNT}")
    kry_files=("${skipped_files[@]}")
fi

echo "🎮 Found ${#kry_files[@]} .kry files to test:"
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
    echo "   📦 Compiling $kry_file to $krb_file..."
    
    if "$KRYON_PATH" compile "$kry_file" -o "$krb_file" ${DEBUG_MODE:+--debug}; then
        echo "   ✅ Compilation successful"
        
        # Run the KRB file with the new modular CLI
        echo "   🖼️  Running with $RENDERER renderer..."
        if "$KRYON_PATH" run "$krb_file" --renderer="$RENDERER" ${DEBUG_MODE:+--debug}; then
            echo "   ✅ Completed: $kry_file"
        else
            echo "   ❌ Failed to run: $kry_file (exit code: $?)"
            echo "   Continue to next example? [y/N]"
            read -r response
            if [[ ! "$response" =~ ^[Yy]$ ]]; then
                echo "🛑 Stopping test run."
                exit 1
            fi
        fi
    else
        echo "   ❌ Failed to compile: $kry_file (exit code: $?)"
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