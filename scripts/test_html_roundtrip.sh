#!/bin/bash
#
# test_html_roundtrip.sh - Test .kir → HTML → .kir roundtrip
#
# Tests that HTML transpilation preserves IR semantics.
# Pipeline: .kir (original) → HTML → .kir (roundtrip) → semantic comparison
#
# Usage:
#   ./scripts/test_html_roundtrip.sh <input.kir>
#   ./scripts/test_html_roundtrip.sh <input.kir> <output_html>
#

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <input.kir> [output.html]"
    echo ""
    echo "Tests HTML roundtrip: .kir → HTML → .kir (with semantic comparison)"
    exit 1
fi

KIR_ORIGINAL="$1"
HTML_OUTPUT="${2:-/tmp/roundtrip_$(basename "$KIR_ORIGINAL" .kir).html}"
KIR_ROUNDTRIP="/tmp/roundtrip_$(basename "$KIR_ORIGINAL" .kir)_roundtrip.kir"

# Check if input file exists
if [ ! -f "$KIR_ORIGINAL" ]; then
    echo -e "${RED}✗ Input file not found: $KIR_ORIGINAL${NC}"
    exit 1
fi

echo "Testing HTML Roundtrip"
echo "======================"
echo "Original KIR:  $KIR_ORIGINAL"
echo "HTML Output:   $HTML_OUTPUT"
echo "Roundtrip KIR: $KIR_ROUNDTRIP"
echo ""

# Step 1: Generate HTML from KIR (transpilation mode)
echo -e "${YELLOW}Step 1: KIR → HTML (transpilation)${NC}"

# Use kryon build command with HTML target
# Note: This requires the IR file to be from a buildable source
# For direct .kir → HTML, we need to use the C test tool for now
if [ -x "$PROJECT_ROOT/build/test_html_transpile" ]; then
    "$PROJECT_ROOT/build/test_html_transpile" "$KIR_ORIGINAL" "$HTML_OUTPUT"
else
    # Fallback: Use kryon CLI if available
    # This is a workaround - ideally we'd have a direct .kir → HTML command
    echo -e "${YELLOW}Warning: Using indirect transpilation method${NC}"

    # For now, skip if we can't find the tool
    echo -e "${RED}✗ HTML transpilation tool not found${NC}"
    echo "   Build with: cd ir && gcc -o ../build/test_html_transpile test_html_transpile.c ../build/libkryon_ir.a -lm -lharfbuzz -lfreetype -lfribidi"
    exit 1
fi

if [ ! -f "$HTML_OUTPUT" ]; then
    echo -e "${RED}✗ HTML generation failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ HTML generated: $HTML_OUTPUT${NC}"
echo ""

# Step 2: Parse HTML back to KIR
echo -e "${YELLOW}Step 2: HTML → KIR (parsing)${NC}"

if [ -x "$PROJECT_ROOT/build/test_html_to_kir" ]; then
    "$PROJECT_ROOT/build/test_html_to_kir" "$HTML_OUTPUT" 2>/dev/null > "$KIR_ROUNDTRIP"
else
    echo -e "${RED}✗ HTML parser tool not found${NC}"
    echo "   Build with: cd ir && gcc -o ../build/test_html_to_kir test_html_to_kir.c ../build/libkryon_ir.a -lm -lharfbuzz -lfreetype -lfribidi"
    exit 1
fi

if [ ! -f "$KIR_ROUNDTRIP" ]; then
    echo -e "${RED}✗ HTML parsing failed${NC}"
    exit 1
fi

echo -e "${GREEN}✓ KIR roundtrip generated: $KIR_ROUNDTRIP${NC}"
echo ""

# Step 3: Compare original and roundtrip KIR (semantic comparison)
echo -e "${YELLOW}Step 3: Comparing KIR files (semantic)${NC}"

if [ -x "$PROJECT_ROOT/scripts/compare_kir.sh" ]; then
    if "$PROJECT_ROOT/scripts/compare_kir.sh" "$KIR_ORIGINAL" "$KIR_ROUNDTRIP"; then
        echo ""
        echo -e "${GREEN}✅ Roundtrip test PASSED${NC}"
        echo -e "${GREEN}   HTML transpilation preserves IR semantics${NC}"
        exit 0
    else
        echo ""
        echo -e "${RED}❌ Roundtrip test FAILED${NC}"
        echo -e "${RED}   Original and roundtrip KIR differ semantically${NC}"
        echo ""
        echo "Original:  $KIR_ORIGINAL"
        echo "Roundtrip: $KIR_ROUNDTRIP"
        echo "HTML:      $HTML_OUTPUT"
        exit 1
    fi
else
    echo -e "${YELLOW}Warning: Semantic comparison tool not found${NC}"
    echo "   Using basic diff instead..."

    if diff -u "$KIR_ORIGINAL" "$KIR_ROUNDTRIP"; then
        echo ""
        echo -e "${GREEN}✅ Roundtrip test PASSED (exact match)${NC}"
        exit 0
    else
        echo ""
        echo -e "${RED}❌ Roundtrip test FAILED${NC}"
        exit 1
    fi
fi
