#!/bin/bash

# Build Web Examples Script
# This script builds all Kryon examples as web pages using the HTML renderer

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🚀 Building Kryon Web Examples${NC}"
echo

# Default values
EXAMPLES_DIR="./examples"
OUTPUT_DIR="./web-examples"
CLEAN=true
SERVE=false
PORT=8080

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --examples-dir)
            EXAMPLES_DIR="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --no-clean)
            CLEAN=false
            shift
            ;;
        --serve)
            SERVE=true
            shift
            ;;
        --port)
            PORT="$2"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  --examples-dir DIR    Directory containing Kryon examples (default: ./examples)"
            echo "  --output DIR         Output directory for web files (default: ./web-examples)"
            echo "  --no-clean           Don't clean output directory before building"
            echo "  --serve              Start development server after building"
            echo "  --port PORT          Port for development server (default: 8080)"
            echo "  --help               Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Check if we're in the right directory
if [ ! -f "Cargo.toml" ] || [ ! -d "examples" ]; then
    echo -e "${RED}❌ This script must be run from the Kryon project root directory${NC}"
    exit 1
fi

# Check if we need to build/rebuild the binary
BINARY_PATH="./target/release/kryon-web-examples"
NEEDS_BUILD=false

if [ ! -f "$BINARY_PATH" ]; then
    echo -e "${YELLOW}📦 Binary not found, building kryon-web-examples...${NC}"
    NEEDS_BUILD=true
else
    # Check if source files are newer than the binary
    if find src crates -name "*.rs" -newer "$BINARY_PATH" | grep -q .; then
        echo -e "${YELLOW}📦 Source files updated, rebuilding kryon-web-examples...${NC}"
        NEEDS_BUILD=true
    else
        echo -e "${GREEN}✅ Using existing kryon-web-examples binary${NC}"
    fi
fi

if [ "$NEEDS_BUILD" = true ]; then
    cargo build --bin kryon-web-examples --features html-server --release
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Failed to build kryon-web-examples binary${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Built kryon-web-examples binary${NC}"
echo

# Run the web examples generator
echo -e "${YELLOW}🔄 Generating web examples...${NC}"

ARGS="--examples-dir $EXAMPLES_DIR --output $OUTPUT_DIR --responsive --accessibility --dark-mode"

if [ "$CLEAN" = true ]; then
    ARGS="$ARGS --clean"
fi

if [ "$SERVE" = true ]; then
    ARGS="$ARGS --serve --port $PORT"
fi

echo "Command: ./target/release/kryon-web-examples $ARGS"
echo

./target/release/kryon-web-examples $ARGS

if [ $? -eq 0 ]; then
    echo
    echo -e "${GREEN}🎉 Successfully generated web examples!${NC}"
    echo -e "${BLUE}📂 Output directory: ${OUTPUT_DIR}${NC}"
    echo -e "${BLUE}🌐 Open ${OUTPUT_DIR}/index.html in your browser to view all examples${NC}"
    
    if [ "$SERVE" = false ]; then
        echo
        echo -e "${YELLOW}💡 Tip: Use --serve to start a development server${NC}"
        echo -e "${YELLOW}   Example: $0 --serve --port 3000${NC}"
    fi
else
    echo -e "${RED}❌ Failed to generate web examples${NC}"
    exit 1
fi