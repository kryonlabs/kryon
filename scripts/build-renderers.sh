#!/usr/bin/env bash

# Build script for Kryon-C renderers
# Builds specific renderers or all renderers with various configurations
#
# Usage:
#   ./build-renderers.sh                     - Build all renderers (debug)
#   ./build-renderers.sh --release           - Build all renderers (release)
#   ./build-renderers.sh --raylib            - Build only raylib renderer
#   ./build-renderers.sh --sdl2              - Build only sdl2 renderer
#   ./build-renderers.sh --html              - Build only html renderer
#   ./build-renderers.sh --release --all     - Build all renderers (release)
#   ./build-renderers.sh --clean             - Clean build directory first

set -e  # Exit on any error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Default settings
BUILD_TYPE="Debug"
BUILD_RAYLIB=false
BUILD_SDL2=false
BUILD_HTML=false
BUILD_ALL=true
CLEAN_BUILD=false

# Parse command line arguments
for arg in "$@"; do
    case "$arg" in
        --release)
            BUILD_TYPE="Release"
            ;;
        --debug)
            BUILD_TYPE="Debug"
            ;;
        --raylib)
            BUILD_RAYLIB=true
            BUILD_ALL=false
            ;;
        --sdl2)
            BUILD_SDL2=true
            BUILD_ALL=false
            ;;
        --html)
            BUILD_HTML=true
            BUILD_ALL=false
            ;;
        --all)
            BUILD_ALL=true
            ;;
        --clean)
            CLEAN_BUILD=true
            ;;
        *)
            echo "Unknown argument: $arg"
            echo "Usage: $0 [--release|--debug] [--raylib|--sdl2|--html|--all] [--clean]"
            exit 1
            ;;
    esac
done

echo "🚀 Kryon-C Renderer Build System"
echo "================================="
echo "Build type: $BUILD_TYPE"
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo ""

# Clean build directory if requested
if $CLEAN_BUILD; then
    echo "🧹 Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake
echo "🔧 Configuring CMake..."
cmake_args=(-DCMAKE_BUILD_TYPE="$BUILD_TYPE")

# Add renderer-specific options
if $BUILD_ALL; then
    echo "📦 Building all renderers..."
    cmake_args+=(-DENABLE_RAYLIB_RENDERER=ON)
    cmake_args+=(-DENABLE_SDL2_RENDERER=ON)
    cmake_args+=(-DENABLE_HTML_RENDERER=ON)
elif $BUILD_RAYLIB; then
    echo "📦 Building raylib renderer..."
    cmake_args+=(-DENABLE_RAYLIB_RENDERER=ON)
    cmake_args+=(-DENABLE_SDL2_RENDERER=OFF)
    cmake_args+=(-DENABLE_HTML_RENDERER=OFF)
elif $BUILD_SDL2; then
    echo "📦 Building SDL2 renderer..."
    cmake_args+=(-DENABLE_RAYLIB_RENDERER=OFF)
    cmake_args+=(-DENABLE_SDL2_RENDERER=ON)
    cmake_args+=(-DENABLE_HTML_RENDERER=OFF)
elif $BUILD_HTML; then
    echo "📦 Building HTML renderer..."
    cmake_args+=(-DENABLE_RAYLIB_RENDERER=OFF)
    cmake_args+=(-DENABLE_SDL2_RENDERER=OFF)
    cmake_args+=(-DENABLE_HTML_RENDERER=ON)
fi

# Run CMake configuration
cmake "${cmake_args[@]}" "$PROJECT_ROOT"

# Build the project
echo "🔨 Building..."
make -j$(nproc)

echo ""
echo "✅ Build completed!"
echo "================================="

# List built binaries
echo "📦 Built binaries:"
if [ -f "$BUILD_DIR/src/cli/kryon-cli" ]; then
    echo "  ✅ kryon-cli: $BUILD_DIR/src/cli/kryon-cli"
else
    echo "  ❌ kryon-cli: not found"
fi

# Check for renderer libraries
renderer_libs_found=false

if [ -f "$BUILD_DIR/src/renderers/libkryon_raylib_renderer.so" ] || [ -f "$BUILD_DIR/src/renderers/libkryon_raylib_renderer.a" ]; then
    echo "  ✅ raylib renderer library"
    renderer_libs_found=true
fi

if [ -f "$BUILD_DIR/src/renderers/libkryon_sdl2_renderer.so" ] || [ -f "$BUILD_DIR/src/renderers/libkryon_sdl2_renderer.a" ]; then
    echo "  ✅ SDL2 renderer library"
    renderer_libs_found=true
fi

if [ -f "$BUILD_DIR/src/renderers/libkryon_html_renderer.so" ] || [ -f "$BUILD_DIR/src/renderers/libkryon_html_renderer.a" ]; then
    echo "  ✅ HTML renderer library"
    renderer_libs_found=true
fi

if ! $renderer_libs_found; then
    echo "  ⚠️  No renderer libraries found (this may be normal depending on your configuration)"
fi

echo ""
echo "🎯 To run examples:"
echo "  ./scripts/run_examples.sh"
echo "  ./scripts/run_examples.sh raylib"
echo "  ./scripts/run_examples.sh sdl2"
echo "  ./scripts/run_examples.sh html"