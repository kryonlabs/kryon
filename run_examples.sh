#!/usr/bin/env bash

# Test script for running all KRB examples with kryon-renderer-raylib
# Compiles .kry files to .krb, then runs each .krb file one by one
#
# Usage:
#   ./run_examples.sh            - Run all examples from the beginning
#   ./run_examples.sh 6          - Skip first 6 examples, start from example 7
#   ./run_examples.sh list       - List all examples with their numbers
#   ./run_examples.sh list 3     - List all examples (skip parameter ignored with list)

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPILER_PATH="$SCRIPT_DIR/target/release/kryc"
RENDERER_PATH="$SCRIPT_DIR/target/release/kryon-renderer-raylib"

# Parse command line arguments
SKIP_COUNT=0
SHOW_LIST=false

# Check arguments
for arg in "$@"; do
    if [ "$arg" = "list" ]; then
        SHOW_LIST=true
    elif [[ "$arg" =~ ^[0-9]+$ ]]; then
        SKIP_COUNT=$arg
    fi
done

# Show list if requested
if $SHOW_LIST; then
    echo "📋 KRY Examples List"
    echo "================================"
    
    # Find all .kry files and display them in order
    krb_files=($(find examples -name "*.kry" -type f ! -path "*/widgets/*" ! -path "*/templates/*" | sort))
    
    if [ ${#krb_files[@]} -eq 0 ]; then
        echo "❌ No .kry files found!"
        exit 1
    fi
    
    echo "Found ${#krb_files[@]} examples:"
    echo ""
    
    for i in "${!krb_files[@]}"; do
        echo "  [$((i+1))] ${krb_files[i]}"
    done
    
    echo ""
    echo "================================"
    exit 0
fi

echo "🚀 KRY Examples Test Runner"
echo "================================"

if [ $SKIP_COUNT -gt 0 ]; then
    echo "⏭️  Will skip first $SKIP_COUNT examples"
    echo ""
fi

# Check if binaries exist and build if needed
if [ ! -f "$COMPILER_PATH" ] || [ ! -f "$RENDERER_PATH" ]; then
    echo "🔧 Building Kryon workspace..."
    cd "$SCRIPT_DIR"
    cargo build --release --workspace
    
    # Copy binaries from kryon workspace to root target
    mkdir -p target/release
    if [ -f "kryon/target/release/kryc" ]; then
        cp "kryon/target/release/kryc" "target/release/"
    fi
    if [ -f "kryon/target/release/kryon-renderer-raylib" ]; then
        cp "kryon/target/release/kryon-renderer-raylib" "target/release/"
    fi
fi

echo "✅ Compiler: $COMPILER_PATH"
echo "✅ Renderer: $RENDERER_PATH"
echo ""

# Find all .kry files and compile them (excluding widgets and templates folders)
echo "📦 Compiling .kry files to .krb..."
find examples -name "*.kry" -type f ! -path "*/widgets/*" ! -path "*/templates/*" | while read -r kry_file; do
    krb_file="${kry_file%.kry}.krb"
    echo "  Compiling: $kry_file -> $krb_file"
    "$COMPILER_PATH" "$kry_file" "$krb_file"
done

echo ""

# Find all .krb files and prepare list (excluding widgets and templates folders)
krb_files=($(find examples -name "*.krb" -type f ! -path "*/widgets/*" ! -path "*/templates/*" | sort))

if [ ${#krb_files[@]} -eq 0 ]; then
    echo "❌ No .krb files found!"
    exit 1
fi

# Apply skip count
if [ $SKIP_COUNT -gt 0 ]; then
    if [ $SKIP_COUNT -ge ${#krb_files[@]} ]; then
        echo "❌ Skip count ($SKIP_COUNT) is greater than or equal to total examples (${#krb_files[@]})"
        exit 1
    fi
    
    # Create new array with skipped elements
    skipped_files=("${krb_files[@]:$SKIP_COUNT}")
    krb_files=("${skipped_files[@]}")
fi

echo "🎮 Found ${#krb_files[@]} .krb files to test:"
for i in "${!krb_files[@]}"; do
    actual_index=$((i + SKIP_COUNT + 1))
    echo "  [$actual_index] ${krb_files[i]}"
done
echo ""

# Run each .krb file
for i in "${!krb_files[@]}"; do
    krb_file="${krb_files[i]}"
    actual_index=$((i + SKIP_COUNT + 1))
    echo "🎯 Running [$actual_index]: $krb_file"
    echo "   (Close the window to continue to next example)"
    
    # Run the renderer with the .krb file
    if "$RENDERER_PATH" "$krb_file"; then
        echo "✅ Completed: $krb_file"
    else
        echo "❌ Failed to run: $krb_file (exit code: $?)"
        echo "   Continue anyway? [y/N]"
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
echo "  Total files tested: ${#krb_files[@]}"
echo "  Compiler: $COMPILER_PATH"  
echo "  Renderer: $RENDERER_PATH"
