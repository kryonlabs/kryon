#!/usr/bin/env bash

# run_example.sh - Run a single Kryon example with specified renderer
# Usage: ./run_example.sh <example_name> <renderer>
# Example: ./run_example.sh button raylib

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
DEBUG_MODE=false
EXAMPLE_NAME=""
RENDERER=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            DEBUG_MODE=true
            shift
            ;;
        -*)
            print_error "Unknown option: $1"
            exit 1
            ;;
        *)
            if [ -z "$EXAMPLE_NAME" ]; then
                EXAMPLE_NAME="$1"
            elif [ -z "$RENDERER" ]; then
                RENDERER="$1"
            else
                print_error "Too many arguments"
                exit 1
            fi
            shift
            ;;
    esac
done

# Check required arguments
if [ -z "$EXAMPLE_NAME" ] || [ -z "$RENDERER" ]; then
    print_error "Usage: $0 [--debug] <example_name> <renderer>"
    print_error "Example: $0 button raylib"
    print_error "Example: $0 --debug hello-world raylib"
    print_error ""
    print_error "Available renderers: raylib, sdl2, html, terminal"
    print_error "Options:"
    print_error "  --debug    Show debug information including KRY file content"
    exit 1
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
EXAMPLES_DIR="$PROJECT_ROOT/examples"
BIN_DIR="$BUILD_DIR/bin"

print_status "Running Kryon example: $EXAMPLE_NAME with $RENDERER renderer"

# Always build the project to ensure we use the latest version
print_status "Building project to ensure latest version..."
if ! "$SCRIPT_DIR/build.sh"; then
    print_error "Failed to build project"
    exit 1
fi

# Check if kryon binary exists after build
KRYON_BIN="$BIN_DIR/kryon"
if [ ! -f "$KRYON_BIN" ]; then
    print_error "Kryon binary not found at $KRYON_BIN even after build"
    print_error "Build may have failed or binary location changed"
    exit 1
fi

# Find the example file
KRY_FILE=""
KRB_FILE=""

# Look for .kry file
if [ -f "$EXAMPLES_DIR/${EXAMPLE_NAME}.kry" ]; then
    KRY_FILE="$EXAMPLES_DIR/${EXAMPLE_NAME}.kry"
    KRB_FILE="$EXAMPLES_DIR/${EXAMPLE_NAME}.krb"
elif [ -f "$EXAMPLES_DIR/widgets/${EXAMPLE_NAME}.kry" ]; then
    KRY_FILE="$EXAMPLES_DIR/widgets/${EXAMPLE_NAME}.kry"
    KRB_FILE="$EXAMPLES_DIR/widgets/${EXAMPLE_NAME}.krb"
else
    print_error "Example file not found: ${EXAMPLE_NAME}.kry"
    print_error "Searched in:"
    print_error "  - $EXAMPLES_DIR/${EXAMPLE_NAME}.kry"
    print_error "  - $EXAMPLES_DIR/widgets/${EXAMPLE_NAME}.kry"
    exit 1
fi

print_status "Found example file: $KRY_FILE"

# Show debug information if requested
if [ "$DEBUG_MODE" = true ]; then
    print_status "=== DEBUG MODE ENABLED ==="
    print_status "Example name: $EXAMPLE_NAME"
    print_status "Renderer: $RENDERER"
    print_status "KRY file: $KRY_FILE"
    print_status "KRB file: $KRB_FILE"
    print_status "Kryon binary: $KRYON_BIN"
    echo ""
    
    print_status "=== KRY FILE CONTENT ==="
    echo -e "${YELLOW}$(cat "$KRY_FILE")${NC}"
    echo ""
    print_status "=== END KRY FILE CONTENT ==="
    echo ""
fi

# Compile the example
print_status "Compiling $KRY_FILE to $KRB_FILE..."
if [ "$DEBUG_MODE" = true ]; then
    print_status "Running compilation command: $KRYON_BIN compile \"$KRY_FILE\" -o \"$KRB_FILE\" --debug"
fi

if [ "$DEBUG_MODE" = true ]; then
    if ! "$KRYON_BIN" compile "$KRY_FILE" -o "$KRB_FILE" --debug; then
        print_error "Failed to compile $KRY_FILE"
        exit 1
    fi
else
    if ! "$KRYON_BIN" compile "$KRY_FILE" -o "$KRB_FILE"; then
        print_error "Failed to compile $KRY_FILE"
        exit 1
    fi
fi

print_success "Compilation completed successfully"

# Check if KRB file was created
if [ ! -f "$KRB_FILE" ]; then
    print_error "KRB file was not created: $KRB_FILE"
    exit 1
fi

# Show KRB file info in debug mode
if [ "$DEBUG_MODE" = true ]; then
    print_status "=== KRB FILE INFO ==="
    if [ -f "$KRB_FILE" ]; then
        print_status "KRB file size: $(stat -c%s "$KRB_FILE") bytes"
        print_status "KRB file created: $(stat -c%y "$KRB_FILE")"
        
        # Show complete hex dump for small files
        KRB_SIZE=$(stat -c%s "$KRB_FILE")
        if [ "$KRB_SIZE" -le 256 ]; then
            print_status "Complete KRB file hex dump:"
            echo -e "${YELLOW}$(hexdump -C "$KRB_FILE")${NC}"
        else
            print_status "KRB file header (first 64 bytes):"
            echo -e "${YELLOW}$(hexdump -C "$KRB_FILE" | head -4)${NC}"
        fi
        
        # Try to inspect the KRB file with kryon inspect command (if available)
        print_status "Attempting to inspect KRB file structure..."
        if "$KRYON_BIN" inspect "$KRB_FILE" 2>/dev/null; then
            print_success "KRB inspection completed"
        else
            print_warning "KRB inspection not available or failed"
            print_status "Analyzing KRB structure manually..."
            
            # Manual analysis of the KRB format
            echo -e "${BLUE}KRB Format Analysis:${NC}"
            echo -e "${YELLOW}Bytes 0-3: $(hexdump -C "$KRB_FILE" | head -1 | cut -c9-20) (Magic: should be 'KRYN')${NC}"
            echo -e "${YELLOW}Bytes 4-7: $(hexdump -C "$KRB_FILE" | head -1 | cut -c21-32) (Version info)${NC}"
            echo -e "${YELLOW}Bytes 8-11: $(hexdump -C "$KRB_FILE" | head -1 | cut -c33-44) (Flags)${NC}"
            echo -e "${YELLOW}Bytes 12-15: $(hexdump -C "$KRB_FILE" | head -1 | cut -c45-56) (Element count)${NC}"
            
            if [ "$KRB_SIZE" -eq 30 ]; then
                print_error "⚠️  PROBLEM DETECTED: KRB file is only 30 bytes!"
                print_error "    This suggests the compiler failed to encode the full KRY structure."
                print_error "    Expected size for the complex button.kry should be much larger."
                print_error "    The @style, @function, and nested elements are missing."
            fi
        fi
    else
        print_warning "KRB file not found: $KRB_FILE"
    fi
    echo ""
fi

# Run the example with specified renderer
print_status "Running $KRB_FILE with $RENDERER renderer..."

if [ "$DEBUG_MODE" = true ]; then
    print_status "Running command: $KRYON_BIN run \"$KRB_FILE\" --renderer $RENDERER"
    echo ""
fi

case "$RENDERER" in
    "raylib")
        if ! "$KRYON_BIN" run "$KRB_FILE" --renderer raylib; then
            print_error "Failed to run example with raylib renderer"
            exit 1
        fi
        ;;
    "sdl2")
        if ! "$KRYON_BIN" run "$KRB_FILE" --renderer sdl2; then
            print_error "Failed to run example with SDL2 renderer"
            exit 1
        fi
        ;;
    "html")
        if ! "$KRYON_BIN" run "$KRB_FILE" --renderer html; then
            print_error "Failed to run example with HTML renderer"
            exit 1
        fi
        ;;
    "terminal")
        if ! "$KRYON_BIN" run "$KRB_FILE" --renderer terminal; then
            print_error "Failed to run example with terminal renderer"
            exit 1
        fi
        ;;
    *)
        print_error "Unknown renderer: $RENDERER"
        print_error "Available renderers: raylib, sdl2, html, terminal"
        exit 1
        ;;
esac

print_success "Example completed successfully!"

# Clean up KRB file (optional - comment out if you want to keep it)
# rm -f "$KRB_FILE"
# print_status "Cleaned up temporary KRB file"