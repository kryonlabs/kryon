#!/usr/bin/env bash

# build.sh - Shared build script for Kryon-C project
# Usage: ./build.sh [options]
# Options:
#   --clean     Clean build directory before building
#   --debug     Build with debug symbols
#   --help      Show this help

set -e

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

# Parse arguments
CLEAN_BUILD=false
BUILD_TYPE="Release"

while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --clean     Clean build directory before building"
            echo "  --debug     Build with debug symbols"
            echo "  --help      Show this help"
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            print_error "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"

print_status "Building Kryon-C project..."
print_status "Project root: $PROJECT_ROOT"
print_status "Build type: $BUILD_TYPE"

# Clean build directory if requested
if [ "$CLEAN_BUILD" = true ]; then
    print_status "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# Create build directory if it doesn't exist
if [ ! -d "$BUILD_DIR" ]; then
    print_status "Creating build directory..."
    mkdir -p "$BUILD_DIR"
fi

cd "$BUILD_DIR"

# Configure with CMake
print_status "Configuring with CMake..."
if ! cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"; then
    print_error "CMake configuration failed"
    exit 1
fi

# Build the project
print_status "Building project..."
if ! make -j$(nproc); then
    print_error "Build failed"
    exit 1
fi

# Verify binary was created
KRYON_BIN="$BUILD_DIR/bin/kryon"
if [ ! -f "$KRYON_BIN" ]; then
    print_error "Kryon binary not found at $KRYON_BIN"
    exit 1
fi

print_success "Build completed successfully!"
print_success "Kryon binary: $KRYON_BIN"