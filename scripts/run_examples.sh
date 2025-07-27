#!/usr/bin/env bash

# Test script for running all KRY examples with the unified kryon binary
# Automatically compiles .kry files and runs them with the appropriate renderer
#
# Usage:
#   ./run_examples.sh                - Run all examples with raylib renderer
#   ./run_examples.sh raylib         - Run all examples with raylib renderer
#   ./run_examples.sh wgpu           - Run all examples with wgpu renderer
#   ./run_examples.sh ratatui        - Run all examples with ratatui renderer
#   ./run_examples.sh 6              - Skip first 6 examples, start from example 7
#   ./run_examples.sh wgpu 6         - Run with wgpu renderer, skip first 6 examples
#   ./run_examples.sh list           - List all examples with their numbers

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Default renderer
RENDERER="raylib"
SKIP_COUNT=0
SHOW_LIST=false

# Parse command line arguments
for arg in "$@"; do
    if [ "$arg" = "list" ]; then
        SHOW_LIST=true
    elif [[ "$arg" =~ ^(raylib|wgpu|sdl2|ratatui|all)$ ]]; then
        RENDERER="$arg"
    elif [[ "$arg" =~ ^[0-9]+$ ]]; then
        SKIP_COUNT=$arg
    fi
done

# Set the appropriate binary path and renderer flag
KRYON_PATH="$SCRIPT_DIR/../target/release/kryon"
RENDERER_FLAG="-r $RENDERER"

# Show list if requested
if $SHOW_LIST; then
    echo "📋 KRY Examples List"
    echo "================================"
    
    # Find all .kry files and display them in order
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

echo "🚀 KRY Examples Test Runner"
echo "================================"

if [ $SKIP_COUNT -gt 0 ]; then
    echo "⏭️  Will skip first $SKIP_COUNT examples"
    echo ""
fi

# Build the modular system
echo "🔧 Building Kryon modular system..."
cd "$SCRIPT_DIR/.."

# First build the main CLI (lightweight, no renderer dependencies)
echo "  📦 Building main kryon CLI..."
cargo build --release --bin kryon

# Then build the specific renderer binary
echo "  📦 Building $RENDERER renderer..."
case "$RENDERER" in
    raylib)
        cargo build --release --bin kryon-renderer-raylib --features raylib
        ;;
    wgpu)
        cargo build --release --bin kryon-renderer-wgpu --features wgpu
        ;;
    sdl2)
        cargo build --release --bin kryon-renderer-sdl2 --features sdl2
        ;;
    ratatui)
        cargo build --release --bin kryon-renderer-ratatui --features ratatui
        ;;
    all)
        echo "  🔨 Building all renderers..."
        "$SCRIPT_DIR/build-renderers.sh" --release --all
        RENDERER="raylib"  # Default to raylib when 'all' is used
        RENDERER_FLAG="-r raylib"
        ;;
    *)
        # Try to build the renderer if it exists
        if cargo build --release --bin kryon-renderer-$RENDERER --features $RENDERER 2>/dev/null; then
            echo "  ✅ Built $RENDERER renderer"
        else
            echo "  ⚠️  Unknown renderer '$RENDERER', will try to use if available"
        fi
        ;;
esac

# Verify binary was created
if [ ! -f "$KRYON_PATH" ]; then
    echo "❌ Kryon build failed: $KRYON_PATH not found"
    exit 1
fi

# Add the target directory to PATH so renderers can be found
export PATH="$SCRIPT_DIR/../target/release:$PATH"

# Verify renderer is available
if ! "$KRYON_PATH" --list-renderers | grep -q "✓.*${RENDERER^}"; then
    echo "⚠️  Warning: $RENDERER renderer might not be available"
    echo "Available renderers:"
    "$KRYON_PATH" --list-renderers | grep "✓"
fi

echo "✅ Kryon CLI: $KRYON_PATH"
echo "✅ Using renderer: $RENDERER"
echo ""

# Find all .kry files (no need to pre-compile, kryon does it automatically)
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

# Run each .kry file with unified kryon binary
for i in "${!kry_files[@]}"; do
    kry_file="${kry_files[i]}"
    actual_index=$((i + SKIP_COUNT + 1))
    total_examples=${#kry_files[@]}
    
    echo "════════════════════════════════════════════════════════════════"
    echo "🎯 Running [$actual_index/$((total_examples + SKIP_COUNT))]: $kry_file"
    echo "════════════════════════════════════════════════════════════════"
    echo ""
    echo "   📝 Description: $(basename "$kry_file" .kry | sed 's/_/ /g' | sed 's/\b\w/\U&/g')"
    echo "   🎮 Instructions: Close the window or press Ctrl+C when done viewing"
    echo "   🚀 Auto-launching in 1 second..."
    
    sleep 1
    
    # Run with unified kryon binary (auto-compiles and runs)
    if "$KRYON_PATH" $RENDERER_FLAG "$kry_file"; then
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
    echo ""
    
    # No delay - immediately continue to next example
done

echo "🎉 All examples completed!"
echo "================================"
echo "Summary:"
echo "  Total files tested: ${#kry_files[@]}"
echo "  Renderer used: $RENDERER"
echo "  Binary: $KRYON_PATH"
