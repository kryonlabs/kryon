#!/usr/bin/env bash

# HTML Accuracy Testing Framework for Kryon
# Validates that .kry files generate exactly expected HTML output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TEST_OUTPUT_DIR="$PROJECT_ROOT/test-output"
EXPECTED_DIR="$PROJECT_ROOT/test-expected"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}🧪 Kryon HTML Accuracy Testing Framework${NC}"
echo "================================================"

# Clean and create test output directory
rm -rf "$TEST_OUTPUT_DIR"
mkdir -p "$TEST_OUTPUT_DIR"
mkdir -p "$EXPECTED_DIR"

# Test case: Hello World
echo -e "${YELLOW}Testing: Hello World example${NC}"

# Generate HTML from .kry file
cd "$PROJECT_ROOT"
cargo run --bin kryon-web-examples --features html-server -- \
    --examples-dir examples/01_Getting_Started \
    --output "$TEST_OUTPUT_DIR" 2>/dev/null

HELLO_WORLD_HTML="$TEST_OUTPUT_DIR/01_Getting_Started/hello_world/index.html"

if [[ ! -f "$HELLO_WORLD_HTML" ]]; then
    echo -e "${RED}❌ FAIL: Hello World HTML not generated${NC}"
    exit 1
fi

echo -e "${GREEN}✅ HTML file generated${NC}"

# Test specific requirements
echo -e "${YELLOW}Checking requirements...${NC}"

# Check container size
CONTAINER_WIDTH=$(grep -o "width: [0-9]*px" "$HELLO_WORLD_HTML" | grep -v "800px" | head -1)
CONTAINER_HEIGHT=$(grep -o "height: [0-9]*px" "$HELLO_WORLD_HTML" | grep -v "600px" | head -1)

if [[ "$CONTAINER_WIDTH" == "width: 200px" ]]; then
    echo -e "${GREEN}✅ Container width: 200px${NC}"
else
    echo -e "${RED}❌ FAIL: Container width expected 200px, got: $CONTAINER_WIDTH${NC}"
    exit 1
fi

if [[ "$CONTAINER_HEIGHT" == "height: 100px" ]]; then
    echo -e "${GREEN}✅ Container height: 100px${NC}"
else
    echo -e "${RED}❌ FAIL: Container height expected 100px, got: $CONTAINER_HEIGHT${NC}"
    exit 1
fi

# Check text alignment
TEXT_ALIGN=$(grep -A10 "kryon-element-3" "$HELLO_WORLD_HTML" | grep "text-align:" | head -1 | sed 's/.*text-align: \([^;]*\);.*/\1/')

if [[ "$TEXT_ALIGN" == "center" ]]; then
    echo -e "${GREEN}✅ Text alignment: center${NC}"
else
    echo -e "${RED}❌ FAIL: Text alignment expected center, got: $TEXT_ALIGN${NC}"
    exit 1
fi

# Check container flex display
CONTAINER_DISPLAY=$(grep -A20 "kryon-element-2" "$HELLO_WORLD_HTML" | grep "display:" | head -1 | sed 's/.*display: \([^;]*\);.*/\1/')

if [[ "$CONTAINER_DISPLAY" == "flex" ]]; then
    echo -e "${GREEN}✅ Container display: flex${NC}"
else
    echo -e "${RED}❌ FAIL: Container display expected flex, got: $CONTAINER_DISPLAY${NC}"
    exit 1
fi

# Check container alignment
CONTAINER_ALIGN=$(grep -A20 "kryon-element-2" "$HELLO_WORLD_HTML" | grep "align-items:" | head -1 | sed 's/.*align-items: \([^;]*\);.*/\1/')

if [[ "$CONTAINER_ALIGN" == "center" ]]; then
    echo -e "${GREEN}✅ Container align-items: center${NC}"
else
    echo -e "${RED}❌ FAIL: Container align-items expected center, got: $CONTAINER_ALIGN${NC}"
    exit 1
fi

# Check that text contains "Hello World"
if grep -q "Hello World" "$HELLO_WORLD_HTML"; then
    echo -e "${GREEN}✅ Text content: Hello World${NC}"
else
    echo -e "${RED}❌ FAIL: Text content missing Hello World${NC}"
    exit 1
fi

# Check no Lua VM when no scripts
if ! grep -q "class KryonLuaVM" "$HELLO_WORLD_HTML"; then
    echo -e "${GREEN}✅ No Lua VM when no scripts${NC}"
else
    echo -e "${RED}❌ FAIL: Lua VM present when no scripts${NC}"
    exit 1
fi

echo -e "${GREEN}🎉 All tests passed!${NC}"
echo "================================================"