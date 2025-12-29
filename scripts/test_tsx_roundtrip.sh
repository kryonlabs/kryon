#!/bin/bash
#
# test_tsx_roundtrip.sh - Test .kry → .kir → .tsx → .kir roundtrip
#
# Tests that TSX transpilation preserves IR semantics including reactive state,
# event handlers, hooks, and text expressions.
#
# Pipeline: .kry → .kir (compile) → .tsx (codegen) → .kir (parse back) → compare
#
# Usage:
#   ./scripts/test_tsx_roundtrip.sh <input.kry>
#   ./scripts/test_tsx_roundtrip.sh --all  # Test all examples
#

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CLI_DIR="$PROJECT_ROOT/cli"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Test all examples if --all flag provided
if [ "$1" == "--all" ]; then
    echo -e "${CYAN}Testing all .kry examples...${NC}"
    echo ""

    PASSED=0
    FAILED=0
    FAILED_FILES=()

    for kry in "$PROJECT_ROOT/examples/kry"/*.kry; do
        if [ -f "$kry" ]; then
            echo -e "${CYAN}========================================${NC}"
            echo -e "${CYAN}Testing: $(basename "$kry")${NC}"
            echo -e "${CYAN}========================================${NC}"

            if "$0" "$kry"; then
                ((PASSED++))
            else
                ((FAILED++))
                FAILED_FILES+=("$(basename "$kry")")
            fi
            echo ""
        fi
    done

    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}Test Summary${NC}"
    echo -e "${CYAN}========================================${NC}"
    echo -e "${GREEN}Passed: $PASSED${NC}"
    if [ $FAILED -gt 0 ]; then
        echo -e "${RED}Failed: $FAILED${NC}"
        echo -e "${RED}Failed files:${NC}"
        for file in "${FAILED_FILES[@]}"; do
            echo -e "${RED}  - $file${NC}"
        done
        exit 1
    else
        echo -e "${GREEN}✅ All tests passed!${NC}"
        exit 0
    fi
fi

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <input.kry>"
    echo "       $0 --all"
    echo ""
    echo "Tests TSX roundtrip: .kry → .kir → .tsx → .kir (with semantic comparison)"
    exit 1
fi

KRY_INPUT="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
BASENAME=$(basename "$KRY_INPUT" .kry)
KIR_ORIGINAL="/tmp/roundtrip_${BASENAME}_original.kir"
TSX_GENERATED="/tmp/roundtrip_${BASENAME}.tsx"
KIR_ROUNDTRIP="/tmp/roundtrip_${BASENAME}_roundtrip.kir"

# Check if input file exists
if [ ! -f "$KRY_INPUT" ]; then
    echo -e "${RED}✗ Input file not found: $KRY_INPUT${NC}"
    exit 1
fi

echo "Testing TSX Roundtrip"
echo "====================="
echo "Input KRY:     $KRY_INPUT"
echo "Original KIR:  $KIR_ORIGINAL"
echo "Generated TSX: $TSX_GENERATED"
echo "Roundtrip KIR: $KIR_ROUNDTRIP"
echo ""

# Step 1: KRY → KIR (compile)
echo -e "${YELLOW}Step 1: Compiling KRY → KIR...${NC}"

cd "$CLI_DIR"

# Compile the .kry file
if ! ./kryon compile "$KRY_INPUT" 2>/dev/null; then
    echo -e "${RED}✗ Compilation failed${NC}"
    exit 1
fi

# Find the generated .kir file in .kryon_cache
if [ ! -f ".kryon_cache/${BASENAME}.kir" ]; then
    echo -e "${RED}✗ Compiled .kir file not found in .kryon_cache/${NC}"
    exit 1
fi

# Copy to temp location for testing
cp ".kryon_cache/${BASENAME}.kir" "$KIR_ORIGINAL"
echo -e "${GREEN}✓ Compiled: $KIR_ORIGINAL${NC}"
echo ""

# Step 2: KIR → TSX (codegen)
echo -e "${YELLOW}Step 2: Generating TSX from KIR...${NC}"

if ! ./kryon codegen tsx "$KIR_ORIGINAL" "$TSX_GENERATED" 2>/dev/null; then
    echo -e "${RED}✗ TSX generation failed${NC}"
    exit 1
fi

if [ ! -f "$TSX_GENERATED" ]; then
    echo -e "${RED}✗ TSX file not created${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Generated: $TSX_GENERATED${NC}"
echo ""

# Step 3: TSX → KIR (parse back)
echo -e "${YELLOW}Step 3: Parsing TSX → KIR...${NC}"

if ! bun tsx_parser/tsx_to_kir.ts "$TSX_GENERATED" > "$KIR_ROUNDTRIP" 2>/dev/null; then
    echo -e "${RED}✗ TSX parsing failed${NC}"
    echo "  Generated TSX may have syntax errors"
    exit 1
fi

if [ ! -f "$KIR_ROUNDTRIP" ]; then
    echo -e "${RED}✗ Roundtrip KIR file not created${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Parsed: $KIR_ROUNDTRIP${NC}"
echo ""

# Step 4: Semantic comparison
echo -e "${YELLOW}Step 4: Comparing KIR files (semantic)...${NC}"
echo ""

# Feature-level validation using jq
echo "Feature Validation:"
echo "==================="

# Check if jq is available
if ! command -v jq &> /dev/null; then
    echo -e "${YELLOW}Warning: jq not found, skipping feature validation${NC}"
else
    # Count state variables
    ORIG_STATE_COUNT=$(jq '.reactive_manifest.variables | length // 0' "$KIR_ORIGINAL" 2>/dev/null || echo "0")
    ROUND_STATE_COUNT=$(jq '.reactive_manifest.variables | length // 0' "$KIR_ROUNDTRIP" 2>/dev/null || echo "0")

    if [ "$ORIG_STATE_COUNT" == "$ROUND_STATE_COUNT" ]; then
        echo -e "${GREEN}✓ State variables: $ORIG_STATE_COUNT${NC}"
    else
        echo -e "${RED}✗ State variables: $ORIG_STATE_COUNT → $ROUND_STATE_COUNT${NC}"
    fi

    # Count event handlers
    ORIG_HANDLER_COUNT=$(jq '.logic_block.functions | length // 0' "$KIR_ORIGINAL" 2>/dev/null || echo "0")
    ROUND_HANDLER_COUNT=$(jq '.logic_block.functions | length // 0' "$KIR_ROUNDTRIP" 2>/dev/null || echo "0")

    if [ "$ORIG_HANDLER_COUNT" == "$ROUND_HANDLER_COUNT" ]; then
        echo -e "${GREEN}✓ Event handlers: $ORIG_HANDLER_COUNT${NC}"
    else
        echo -e "${RED}✗ Event handlers: $ORIG_HANDLER_COUNT → $ROUND_HANDLER_COUNT${NC}"
    fi

    # Count hooks
    ORIG_HOOK_COUNT=$(jq '.reactive_manifest.hooks | length // 0' "$KIR_ORIGINAL" 2>/dev/null || echo "0")
    ROUND_HOOK_COUNT=$(jq '.reactive_manifest.hooks | length // 0' "$KIR_ROUNDTRIP" 2>/dev/null || echo "0")

    if [ "$ORIG_HOOK_COUNT" == "$ROUND_HOOK_COUNT" ]; then
        echo -e "${GREEN}✓ Hooks: $ORIG_HOOK_COUNT${NC}"
    else
        echo -e "${RED}✗ Hooks: $ORIG_HOOK_COUNT → $ROUND_HOOK_COUNT${NC}"
    fi

    # Count components in root
    ORIG_COMP_COUNT=$(jq '.. | objects | select(has("type")) | .type' "$KIR_ORIGINAL" 2>/dev/null | wc -l)
    ROUND_COMP_COUNT=$(jq '.. | objects | select(has("type")) | .type' "$KIR_ROUNDTRIP" 2>/dev/null | wc -l)

    if [ "$ORIG_COMP_COUNT" == "$ROUND_COMP_COUNT" ]; then
        echo -e "${GREEN}✓ Components: $ORIG_COMP_COUNT${NC}"
    else
        echo -e "${RED}✗ Components: $ORIG_COMP_COUNT → $ROUND_COMP_COUNT${NC}"
    fi
fi

echo ""
echo "JSON Diff:"
echo "=========="

# Pretty-print both files for comparison
if command -v jq &> /dev/null; then
    jq --sort-keys . "$KIR_ORIGINAL" > /tmp/orig_pretty.json 2>/dev/null || cp "$KIR_ORIGINAL" /tmp/orig_pretty.json
    jq --sort-keys . "$KIR_ROUNDTRIP" > /tmp/round_pretty.json 2>/dev/null || cp "$KIR_ROUNDTRIP" /tmp/round_pretty.json

    if diff -u /tmp/orig_pretty.json /tmp/round_pretty.json > /tmp/roundtrip_diff.txt; then
        echo -e "${GREEN}✓ Files are identical${NC}"
        echo ""
        echo -e "${GREEN}✅ ROUNDTRIP TEST PASSED${NC}"
        echo -e "${GREEN}   TSX transpilation preserves IR semantics${NC}"
        exit 0
    else
        echo -e "${YELLOW}⚠ Files differ (showing first 50 lines)${NC}"
        head -50 /tmp/roundtrip_diff.txt
        echo ""
        echo -e "${YELLOW}Full diff available at: /tmp/roundtrip_diff.txt${NC}"
        echo ""

        # Check if differences are only in metadata/IDs (acceptable)
        SEMANTIC_DIFF=$(diff -u /tmp/orig_pretty.json /tmp/round_pretty.json | grep -E '^\+|^-' | grep -v '^\+\+\+' | grep -v '^---' | grep -v '"id"' | grep -v '"metadata"' | wc -l)

        if [ "$SEMANTIC_DIFF" -eq 0 ]; then
            echo -e "${GREEN}✅ ROUNDTRIP TEST PASSED (differences only in IDs/metadata)${NC}"
            exit 0
        else
            echo -e "${YELLOW}⚠ ROUNDTRIP TEST COMPLETED WITH DIFFERENCES${NC}"
            echo "   Original:  $KIR_ORIGINAL"
            echo "   Roundtrip: $KIR_ROUNDTRIP"
            echo "   TSX:       $TSX_GENERATED"
            exit 1
        fi
    fi
else
    # Fallback to basic diff if jq not available
    if diff -u "$KIR_ORIGINAL" "$KIR_ROUNDTRIP"; then
        echo -e "${GREEN}✅ ROUNDTRIP TEST PASSED (exact match)${NC}"
        exit 0
    else
        echo -e "${RED}❌ ROUNDTRIP TEST FAILED${NC}"
        exit 1
    fi
fi
