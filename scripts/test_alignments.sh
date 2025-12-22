#!/bin/bash
# Test all alignment modes (Phase 4.2)
# This script validates that the refactored layout system works correctly

set -e  # Exit on error (errors are now fatal with the new validation!)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "====================================="
echo "Testing Kryon Alignment System"
echo "====================================="
echo ""

# Test row alignments
echo "Testing row alignments..."
cd "$PROJECT_ROOT"
if bash run_example.sh row_alignments nim terminal; then
    echo "✓ Row alignments passed"
else
    echo "✗ Row alignments FAILED"
    exit 1
fi
echo ""

# Test column alignments
echo "Testing column alignments..."
cd "$PROJECT_ROOT"
if bash run_example.sh column_alignments nim terminal; then
    echo "✓ Column alignments passed"
else
    echo "✗ Column alignments FAILED"
    exit 1
fi
echo ""

echo "====================================="
echo "All alignment tests passed!"
echo "====================================="
echo ""
echo "The layout system correctly handles:"
echo "  - START (flex-start)"
echo "  - CENTER"
echo "  - END (flex-end)"
echo "  - SPACE_BETWEEN"
echo "  - SPACE_AROUND"
echo "  - SPACE_EVENLY"
echo ""
