#!/usr/bin/env bash

# build_web_example.sh - Build a Kryon example to web (HTML/CSS/JS)
# Usage: ./build_web_example.sh <example_name> [output_dir]
# Example: ./build_web_example.sh button
# Example: ./build_web_example.sh hello-world /tmp/my-web-app

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
EXAMPLE_NAME=""
OUTPUT_DIR=""

if [ $# -eq 0 ]; then
    print_error "Usage: $0 <example_name> [output_dir]"
    print_error "Example: $0 button"
    print_error "Example: $0 hello-world /tmp/my-web-app"
    print_error ""
    print_error "If output_dir is not specified, will use: web_output/<example_name>/"
    exit 1
fi

EXAMPLE_NAME="$1"
if [ -n "$2" ]; then
    OUTPUT_DIR="$2"
else
    OUTPUT_DIR="web_output/${EXAMPLE_NAME}"
fi

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build"
EXAMPLES_DIR="$PROJECT_ROOT/examples"
BIN_DIR="$BUILD_DIR/bin"
KRYON_BIN="$BIN_DIR/kryon"

print_status "Building web output for: $EXAMPLE_NAME"

# Build project if kryon binary doesn't exist
if [ ! -f "$KRYON_BIN" ]; then
    print_status "Building project..."
    if ! "$SCRIPT_DIR/build.sh"; then
        print_error "Failed to build project"
        exit 1
    fi
fi

# Check if kryon binary exists
if [ ! -f "$KRYON_BIN" ]; then
    print_error "Kryon binary not found at $KRYON_BIN"
    exit 1
fi

# Find the example file
KRY_FILE=""
KRB_FILE=""

# Look for .kry file
if [ -f "$EXAMPLES_DIR/${EXAMPLE_NAME}.kry" ]; then
    KRY_FILE="$EXAMPLES_DIR/${EXAMPLE_NAME}.kry"
    KRB_FILE="$EXAMPLES_DIR/${EXAMPLE_NAME}.krb"
elif [ -f "$EXAMPLES_DIR/debug/${EXAMPLE_NAME}.kry" ]; then
    KRY_FILE="$EXAMPLES_DIR/debug/${EXAMPLE_NAME}.kry"
    KRB_FILE="$EXAMPLES_DIR/debug/${EXAMPLE_NAME}.krb"
else
    print_error "Example file not found: ${EXAMPLE_NAME}.kry"
    print_error "Searched in:"
    print_error "  - $EXAMPLES_DIR/${EXAMPLE_NAME}.kry"
    print_error "  - $EXAMPLES_DIR/debug/${EXAMPLE_NAME}.kry"
    exit 1
fi

print_status "Found example file: $KRY_FILE"

# Compile the example
print_status "Compiling $KRY_FILE to $KRB_FILE..."
if ! "$KRYON_BIN" compile "$KRY_FILE" -o "$KRB_FILE"; then
    print_error "Failed to compile $KRY_FILE"
    exit 1
fi

print_success "Compilation completed successfully"

# Check if KRB file was created
if [ ! -f "$KRB_FILE" ]; then
    print_error "KRB file was not created: $KRB_FILE"
    exit 1
fi

# Create output directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

# Generate web output
print_status "Generating web output to: $OUTPUT_DIR"
if ! "$KRYON_BIN" run "$KRB_FILE" --renderer web --output "$OUTPUT_DIR"; then
    print_error "Failed to generate web output"
    exit 1
fi

print_success "Web output generated successfully!"
print_status ""
print_status "📁 Output directory: $OUTPUT_DIR"
print_status "📄 Files generated:"
print_status "   - index.html  (main HTML file)"
print_status "   - styles.css  (styles)"
print_status "   - runtime.js  (JavaScript runtime)"
print_status ""
print_success "To view the app, open index.html in a web browser:"
print_success "  file://$OUTPUT_DIR/index.html"
print_status ""
print_status "Or use a local web server (recommended):"
print_status "  cd $OUTPUT_DIR && python3 -m http.server 8000"
print_status "  Then open: http://localhost:8000"